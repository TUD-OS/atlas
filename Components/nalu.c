/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "libavcodec/avcodec.h"
#include "nalu.h"

#pragma clang diagnostic ignored "-Wpadded"

struct nalu_read_s {
	const uint8_t * restrict nalu;
	uint_fast8_t byte;
	uint_fast8_t bits;
};

struct nalu_write_s {
	FILE *from, *to;
	uint8_t buf[4096];
	uint8_t history[3];
	uint8_t byte;
	uint_fast8_t bits;
};

static inline uint32_t reverse_bits(uint32_t x)
{
	x = ((x >>  1) & 0x55555555u) | ((x & 0x55555555u) <<  1);
	x = ((x >>  2) & 0x33333333u) | ((x & 0x33333333u) <<  2);
	x = ((x >>  4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) <<  4);
	x = ((x >>  8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) <<  8);
	x = ((x >> 16) & 0x0000ffffu) | ((x & 0x0000ffffu) << 16);
	return x;
}

#pragma mark -


#pragma mark Metadata NALU Reading

nalu_read_t *nalu_read_alloc(void)
{
	return malloc(sizeof(nalu_read_t));
}

void nalu_read_start(nalu_read_t *read, const uint8_t *nalu)
{
	read->nalu = nalu;
	read->byte = *nalu;
	read->bits = 8;
}

static inline uint_fast8_t nalu_read_bit(nalu_read_t *read)
{
	uint_fast8_t bit = (read->byte >> 7) & 1;
	read->byte <<= 1;
	if (!(--read->bits)) {
		read->nalu++;
		read->byte = *read->nalu;
		read->bits = 8;
	}
	return bit;
}

uint_fast32_t nalu_read_unsigned(nalu_read_t *read)
{
	size_t leading_zeros = 0;
	while (nalu_read_bit(read) == 0)
		leading_zeros++;
	assert(leading_zeros < 32);
	
	uint_fast32_t coded = 1;
	while (leading_zeros--)
		coded = (coded << 1) | nalu_read_bit(read);
	
	return coded - 1;
}

int_fast32_t nalu_read_signed(nalu_read_t *read)
{
	uint_fast32_t coded = nalu_read_unsigned(read);
	return (coded & 1) ? (coded >> 1) + 1 : -(coded >> 1);
}

float nalu_read_float(nalu_read_t *read)
{
	uint32_t coded = (uint32_t)nalu_read_unsigned(read);
	int32_t fixedpoint = (int32_t)reverse_bits(coded);
	int32_t exponent = fixedpoint ? (int32_t)nalu_read_signed(read) : 0;
	return ldexpf((float)fixedpoint, exponent - 31);
}

void nalu_read_free(nalu_read_t *read)
{
	free(read);
}

#pragma mark -


#pragma mark Metadata NALU Writing

nalu_write_t *nalu_write_alloc(const char *source)
{
	if (strcmp(&source[strlen(source) - sizeof(".h264") + 1], ".h264") != 0) {
		fprintf(stderr, "filename does not have the proper .h264 ending\n");
		abort();
	}
	
	nalu_write_t *write = malloc(sizeof(nalu_write_t));
	if (!write) return NULL;
	
	char target[strlen(source) + sizeof("_" FILE_SUFFIX)];
	sprintf(target, "%s_" FILE_SUFFIX, source);
	write->from = fopen(source, "r");
	write->to = fopen(target, "w");
	
	return write;
}

void nalu_write_start(nalu_write_t *write)
{
	const uint8_t custom[4] = { 0x00, 0x00, 0x01, NAL_METADATA };
	
	fwrite(custom, 1, 4, write->to);
	write->history[2] = 0xFF;
	write->byte = 0;
	write->bits = 0;
}

static inline void nalu_write_bit(nalu_write_t *write, uint_fast8_t bit)
{
	assert(bit == 0 || bit == 1);
	
	write->byte <<= 1;
	write->byte |= bit;
	if (++write->bits == 8) {
		const uint8_t fill = 0x03;
		
		write->history[0] = write->history[1];
		write->history[1] = write->history[2];
		write->history[2] = write->byte;
		/* the three byte sequences 0x000001, 0x000002 and 0x000003 are special in H.264 NAL;
		 * if they do appear regularly, they must be disguised by inserting an extra 0x03 */
		if (write->history[0] == 0 && write->history[1] == 0 && write->history[2] < 4) {
			fwrite(&fill, 1, 1, write->to);
			write->history[1] = 0xFF;
		}
		
		fwrite(&write->byte, 1, 1, write->to);
		write->byte = 0;
		write->bits = 0;
	}
}

void nalu_write_unsigned(nalu_write_t *write, uint_fast32_t value)
{
	assert(value < (1u << 31));
	value = value + 1;
	
	uint_fast32_t temp = value;
	size_t significant_bits = 0;
	while (temp) {
		significant_bits++;
		temp >>= 1;
	}
	for (size_t i = 1; i < significant_bits; i++)
		nalu_write_bit(write, 0);
	
	while (significant_bits--)
		nalu_write_bit(write, (value >> significant_bits) & 1);
}

void nalu_write_signed(nalu_write_t *write, int_fast32_t value)
{
	uint_fast32_t coded = (value > 0) ? ((uint_fast32_t)value << 1) - 1 : (uint_fast32_t)(-value) << 1;
	nalu_write_unsigned(write, coded);
}

void nalu_write_float(nalu_write_t *write, float value)
{
	int exponent;
	float magnitude = frexpf(value, &exponent);
	assert(isfinite(magnitude));
	int32_t fixedpoint = (int32_t)(ldexpf(magnitude, 31));
	
	/* reverse because lower bits are more likely zero */
	uint32_t coded = reverse_bits((uint32_t)fixedpoint);
	nalu_write_unsigned(write, coded);
	if (fixedpoint)
		nalu_write_signed(write, exponent);
}

void nalu_write_end(nalu_write_t *write)
{
	/* flush all remaining bits */
	while (write->bits)
		nalu_write_bit(write, 0);
}

void nalu_write_free(nalu_write_t *write)
{
	free(write);
}

#pragma mark -


#pragma mark NALU Copying

bool check_slice_start(nalu_write_t *write)
{
	while (1) {
		/* peek ahead to get the start code */
		if (fread(write->buf, 1, 4, write->from) != 4) {
			fprintf(stderr, "could not read NALU start code\n");
			fprintf(stderr, "source file position %jd\n", (intmax_t)ftello(write->from));
			fprintf(stderr, "target file position %jd\n", (intmax_t)ftello(write->to));
			abort();
		}
		fseek(write->from, -4, SEEK_CUR);
		if (write->buf[0] == 0 && write->buf[1] == 0 && write->buf[2] == 1)
			break;
		else
			return false;
	}
	return ((write->buf[3] & 0x1F) > 0 && (write->buf[3] & 0x1F) < 6);
}

void copy_nalu(nalu_write_t *write)
{
	size_t write_bytes, read_bytes;
	/* skip our own NALUs, if the file has already been preprocessed */
	const int skip =
		(write->buf[0] == 0) &&
		(write->buf[1] == 0) &&
		(write->buf[2] == 1) &&
		((write->buf[3] & 0x1F) == NAL_METADATA);
	
	/* the start bytes are alredy in the buffer, take them from there and skip them in the file */
	if (!skip)
		fwrite(write->buf, 1, 4, write->to);
	fseek(write->from, 4, SEEK_CUR);
	do {
		read_bytes = fread(write->buf, 1, sizeof(write->buf), write->from);
		for (write_bytes = 0; write_bytes < read_bytes - 2; write_bytes++)
			if (write->buf[write_bytes] == 0 && write->buf[write_bytes+1] == 0 && write->buf[write_bytes+2] == 1)
				break;
		if (feof(write->from) && write_bytes == read_bytes - 2)
			// last NALU, write everything
			write_bytes = read_bytes;
		if (!skip)
			fwrite(write->buf, 1, write_bytes, write->to);
		fseek(write->from, (long)(write_bytes - read_bytes), SEEK_CUR);
	} while (write_bytes == read_bytes - 2);
}

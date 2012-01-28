/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libavcodec/avcodec.h"

#include "nalu.h"

struct nalu_read_s {
	const uint8_t * restrict nalu;
	uint8_t byte;
	uint8_t bits;
};

struct nalu_write_s {
	FILE *from, *to;
	uint8_t buf[4096];
	uint8_t history[3];
	uint32_t remain;
	uint8_t remain_bits;
};


/* sideband data is compressed using Fibonacci coding,
 * see http://en.wikipedia.org/wiki/Fibonacci_coding for details */
static const uint8_t fibonacci[] = {
	1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233
};


#pragma mark Sideband NALU Reading

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

uint8_t nalu_read_uint8(nalu_read_t *read)
{
	return (uint8_t)nalu_read_int8(read);
}

uint16_t nalu_read_uint16(nalu_read_t *read)
{
	/* big endian stream */
	uint16_t value = 0;
	value |= ((uint16_t)nalu_read_uint8(read) << 8);
	value |= ((uint16_t)nalu_read_uint8(read) << 0);
	return value;
}

uint32_t nalu_read_uint24(nalu_read_t *read)
{
	/* big endian stream */
	uint32_t value = 0;
	value |= ((uint32_t)nalu_read_uint8(read) << 16);
	value |= ((uint32_t)nalu_read_uint8(read) <<  8);
	value |= ((uint32_t)nalu_read_uint8(read) <<  0);
	return value;
}

uint32_t nalu_read_uint32(nalu_read_t *read)
{
	/* big endian stream */
	uint32_t value = 0;
	value |= ((uint32_t)nalu_read_uint8(read) << 24);
	value |= ((uint32_t)nalu_read_uint8(read) << 16);
	value |= ((uint32_t)nalu_read_uint8(read) <<  8);
	value |= ((uint32_t)nalu_read_uint8(read) <<  0);
	return value;
}

int8_t nalu_read_int8(nalu_read_t *read)
{
	unsigned coded = 0;
	int i, bit, last_bit = 0;
	
	for (i = 0; i < sizeof(fibonacci) / sizeof(fibonacci[0]); i++) {
		bit = read->byte & (1 << 7);
		read->byte <<= 1;
		if (!(--read->bits)) {
			read->nalu++;
			read->byte = *read->nalu;
			read->bits = 8;
		}
		if (bit && last_bit) break;
		if (bit) coded += fibonacci[i];
		last_bit = bit;
	}
	/* map to positive and negative integers with zero */
	if (coded & 1)
		return (int8_t)-((coded - 1) / 2);
	else
		return (int8_t)(coded / 2);
}

int16_t nalu_read_int16(nalu_read_t *read)
{
	return (int16_t)nalu_read_uint16(read);
}

float nalu_read_float(nalu_read_t *read)
{
	union {
		uint32_t in;
		float out;
	} convert;
	assert(sizeof(float) == sizeof(uint32_t));
	convert.in = nalu_read_uint32(read);
	return convert.out;
}

void nalu_read_free(nalu_read_t *read)
{
	free(read);
}

#pragma mark -


#pragma mark Sideband NALU Writing

nalu_write_t *nalu_write_alloc(const char *source)
{
	if (strcmp(&source[strlen(source) - sizeof(".h264") + 1], ".h264") != 0) {
		fprintf(stderr, "filename does not have the proper .h264 ending\n");
		abort();
	}
	
	nalu_write_t *write = malloc(sizeof(nalu_write_t));
	if (!write) return NULL;
	
	char *target = strdup(source);
	target[strlen(target) - sizeof("h264") + 1] = 'p';
	write->from = fopen(source, "r");
	write->to = fopen(target, "w");
	free(target);
	
	return write;
}

void nalu_write_start(nalu_write_t *write)
{
	const uint8_t custom[4] = { 0x00, 0x00, 0x01, NAL_SIDEBAND_DATA };
	
	fwrite(custom, 1, 4, write->to);
	write->history[2] = 0xFF;
	write->remain = 0;
	write->remain_bits = 0;
}

static void nalu_write_byte(nalu_write_t *write, uint8_t value)
{
	const uint8_t fill = 0x03;
	
	write->history[0] = write->history[1];
	write->history[1] = write->history[2];
	write->history[2] = value;
	/* the three byte sequences 0x000001, 0x000002 and 0x000003 are special in H.264 NAL;
	 * if they do appear regularly, they must be disguised by inserting an extra 0x03 */
	if (write->history[0] == 0 && write->history[1] == 0 && write->history[2] < 4) {
		fwrite(&fill, 1, 1, write->to);
		write->history[1] = 0xFF;
	}
	fwrite(&value, 1, 1, write->to);
}

void nalu_write_end(nalu_write_t *write)
{
	uint32_t coded = write->remain;
	int bits = write->remain_bits;
	/* flush all remaining bits */
	for (; bits > 0; bits -= 8, coded <<= 8)
		nalu_write_byte(write, (uint8_t)(coded >> 24));
}

void nalu_write_uint8(nalu_write_t *write, uint8_t value)
{
	nalu_write_int8(write, (int8_t)value);
}

void nalu_write_uint16(nalu_write_t *write, uint16_t value)
{
	/* big endian stream */
	nalu_write_uint8(write, (value >> 8) & 0xFF);
	nalu_write_uint8(write, (value >> 0) & 0xFF);
}

void nalu_write_uint24(nalu_write_t *write, uint32_t value)
{
	/* big endian stream */
	nalu_write_uint8(write, (value >> 16) & 0xFF);
	nalu_write_uint8(write, (value >>  8) & 0xFF);
	nalu_write_uint8(write, (value >>  0) & 0xFF);
}

void nalu_write_uint32(nalu_write_t *write, uint32_t value)
{
	/* big endian stream */
	nalu_write_uint8(write, (value >> 24) & 0xFF);
	nalu_write_uint8(write, (value >> 16) & 0xFF);
	nalu_write_uint8(write, (value >>  8) & 0xFF);
	nalu_write_uint8(write, (value >>  0) & 0xFF);
}

void nalu_write_int8(nalu_write_t *write, int8_t value)
{
	/* map to positive values greater zero */
	unsigned normalized =
		((value <  0) ? (2 * -(int)value + 1) :
		 ((value == 0) ? 1 :
		  ((value >  0) ? (2 * (int)value) : 0)));
	/* create the coded number in the higher bits in reverse */
	uint32_t coded;
	int i, bits;
	
	/* search for the highest contribution */
	for (i = sizeof(fibonacci) / sizeof(fibonacci[0]) - 1; i >= 0; i--)
		if (fibonacci[i] <= normalized) break;
	/* bit count is one more than i because i starts counting with zero
	 * and maybe an additional one more because of the one bit already in coded */
	if (i == sizeof(fibonacci) / sizeof(fibonacci[0]) - 1) {
		/* we can omit the end marker if the number is of maximal bitlength */
		coded = 0;
		bits = i + 1;
	} else {
		coded = (1 << 31);
		bits = i + 2;
	}
	for (; i >= 0; i--) {
		coded >>= 1;
		if (fibonacci[i] <= normalized) {
			coded |= (1 << 31);
			normalized -= fibonacci[i];
		}
	}
	assert(normalized == 0);
	/* merge with not yet written bits */
	bits += write->remain_bits;
	coded >>= write->remain_bits;
	coded |= write->remain;
	/* write all completed bytes */
	for (; bits >= 8; bits -= 8, coded <<= 8)
		nalu_write_byte(write, (uint8_t)(coded >> 24));
	/* remember the remainder for later */
	write->remain_bits = bits;
	write->remain = coded;
}

void nalu_write_int16(nalu_write_t *write, int16_t value)
{
	nalu_write_uint16(write, (uint16_t)value);
}

void nalu_write_float(nalu_write_t *write, float value)
{
	union {
		float in;
		uint32_t out;
	} convert;
	assert(sizeof(float) == sizeof(uint32_t));
	convert.in = value;
	nalu_write_uint32(write, convert.out);
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
			fprintf(stderr, "source file position %lld\n", ftello(write->from));
			fprintf(stderr, "target file position %lld\n", ftello(write->to));
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
	int write_bytes, read_bytes;
	/* skip our own NALUs, if the file has already been preprocessed */
	const int skip =
		(write->buf[0] == 0) &&
		(write->buf[1] == 0) &&
		(write->buf[2] == 1) &&
		((write->buf[3] & 0x1F) == NAL_SIDEBAND_DATA);
	
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
		fseek(write->from, write_bytes - read_bytes, SEEK_CUR);
	} while (write_bytes == read_bytes - 2);
}

/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdbool.h>

#ifndef FILE_SUFFIX
#define FILE_SUFFIX "metrics"
#endif

/* opaque handles for NALU access */
typedef struct nalu_read_s nalu_read_t;
typedef struct nalu_write_s nalu_write_t;

/* metadata NALU reading */
nalu_read_t *nalu_read_alloc(void);
void nalu_read_start(nalu_read_t *read, const uint8_t *nalu);
uint8_t nalu_read_uint8(nalu_read_t *read);
uint16_t nalu_read_uint16(nalu_read_t *read);
uint32_t nalu_read_uint24(nalu_read_t *read);
uint32_t nalu_read_uint32(nalu_read_t *read);
int8_t nalu_read_int8(nalu_read_t *read);
int16_t nalu_read_int16(nalu_read_t *read);
float nalu_read_float(nalu_read_t *read);
void nalu_read_free(nalu_read_t *read);

/* metadata NALU writing */
nalu_write_t *nalu_write_alloc(const char *filename);
void nalu_write_start(nalu_write_t *write);
void nalu_write_end(nalu_write_t *write);
void nalu_write_uint8(nalu_write_t *write, uint8_t value);
void nalu_write_uint16(nalu_write_t *write, uint16_t value);
void nalu_write_uint24(nalu_write_t *write, uint32_t value);
void nalu_write_uint32(nalu_write_t *write, uint32_t value);
void nalu_write_int8(nalu_write_t *write, int8_t value);
void nalu_write_int16(nalu_write_t *write, int16_t value);
void nalu_write_float(nalu_write_t *write, float value);
void nalu_write_free(nalu_write_t *write);

/* NALU copying */
bool check_slice_start(nalu_write_t *write);
void copy_nalu(nalu_write_t *write);

/*
 * Copyright (C) 2006-2015 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef FILE_SUFFIX
#define FILE_SUFFIX "metrics"
#endif

/* opaque handles for NALU access */
typedef struct nalu_read_s nalu_read_t;
typedef struct nalu_write_s nalu_write_t;

/* metadata storage uses exp-golomb coding */

/* metadata NALU reading */
nalu_read_t *nalu_read_alloc(void);
void nalu_read_start(nalu_read_t *read, const uint8_t *nalu);
uint_fast32_t nalu_read_unsigned(nalu_read_t *read);
int_fast32_t nalu_read_signed(nalu_read_t *read);
float nalu_read_float(nalu_read_t *read);
void nalu_read_free(nalu_read_t *read);

/* metadata NALU writing */
nalu_write_t *nalu_write_alloc(const char *filename);
void nalu_write_start(nalu_write_t *write);
void nalu_write_unsigned(nalu_write_t *write, uint_fast32_t value);
void nalu_write_signed(nalu_write_t *write, int_fast32_t value);
void nalu_write_float(nalu_write_t *write, float value);
void nalu_write_end(nalu_write_t *write);
void nalu_write_free(nalu_write_t *write);

/* NALU copying */
bool check_slice_start(nalu_write_t *write);
void copy_nalu(nalu_write_t *write);

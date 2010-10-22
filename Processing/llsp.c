/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "llsp.h"

/* the amount of metrics values is currently limited, because a bitfield
 * is used in llsp_solve() to store, which columns to use and which to drop;
 * this line also assumes octets (8 bits per byte) */
#define MAX_METRICS (sizeof(unsigned long long) * 8)


/* internal structures */
typedef struct learning_s learning_t;
typedef struct metrics_node_s metrics_node_t;
typedef struct matrix_s matrix_t;

struct llsp_s {
	int              id;		/* context id */
	unsigned         columns;	/* columns of the collected (metrics, time) tuples */
	learning_t      *learn;	/* for learning phase */
	double           result[1];	/* the resulting coefficients (array will be sufficiently large) */
};

struct learning_s {
	unsigned         total_rows;	/* total amount of collected tuples */
	metrics_node_t  *head;	/* the first tuple block */
	metrics_node_t **list_end;	/* the end of the block list for easy appending */
	double          *cur;		/* the next sample to write to */
	double          *last;	/* the last sample in the tuple block currently being written */  
};

struct metrics_node_s {
	unsigned        rows;
	metrics_node_t *next;
	/* this array will be larger, because we allocate additional space for the struct */
	double          metrics[1];
};

struct matrix_s {
	unsigned  rows;
	unsigned  columns;
	/* this array will be larger, because we allocate additional space for the struct */
	double   *value[1];	/* Attention! matrix is transposed for efficient column-wise access */
};

/* helper functions */
static int llsp_solve(llsp_t *llsp);
static matrix_t *llsp_copy_to_matrix(llsp_t *llsp, const unsigned long long used_columns);
static void matrix_qr_decompose(matrix_t *matrix, char pivoting);
static long double scalarprod(double *a, double *b, unsigned rows);
static void matrix_trisolve(matrix_t *matrix);
static void matrix_dispose(matrix_t *matrix);
static void llsp_dispose_learn(llsp_t *llsp);

#ifdef __GNUC__
static void llsp_dump(llsp_t *llsp) __attribute__((unused));
static void matrix_dump(matrix_t *matrix) __attribute__((unused));
#else
static void llsp_dump(llsp_t *llsp);
static void matrix_dump(matrix_t *matrix);
#endif


llsp_t *llsp_new(int id, unsigned count)
{
	llsp_t *llsp;
	
	if (count < 1 || count > MAX_METRICS) return NULL;
	
	llsp = (llsp_t *)malloc(sizeof(llsp_t) + (count - 1) * sizeof(double));
	if (!llsp) return NULL;
	
	llsp->id      = id;
	llsp->columns = count + 1;  /* we store the time as the last column */
	llsp->learn   = NULL;
	
	return llsp;
}

void llsp_accumulate(llsp_t *llsp, const double *metrics, double time)
{
	unsigned i;
	
	if (!llsp->learn) {
		llsp->learn = (learning_t *)malloc(sizeof(learning_t));
		llsp->learn->total_rows = 0;
		llsp->learn->head       = NULL;
		llsp->learn->list_end   = &llsp->learn->head;
		llsp->learn->cur        = NULL;
		llsp->learn->last       = NULL;
	}
	
	if (llsp->learn->cur == llsp->learn->last) {
		/* no place to write the next row, we need to allocate a new node */
		/* we allocate a pseudo-random amount of rows to avoid any aliasing problems */
		unsigned rows = 1024 + ((double)rand() / (double)RAND_MAX) * 1024.0;
		size_t size = sizeof(metrics_node_t) + (rows * llsp->columns - 1) * sizeof(double);
		*llsp->learn->list_end = (metrics_node_t *)malloc(size);
		if (!*llsp->learn->list_end) return;  /* we'll have to make due with what we have */
		(*llsp->learn->list_end)->rows = rows;
		(*llsp->learn->list_end)->next = NULL;
		llsp->learn->cur      = (*llsp->learn->list_end)->metrics;
		llsp->learn->last     = (*llsp->learn->list_end)->metrics + rows * llsp->columns;
		llsp->learn->list_end = &(*llsp->learn->list_end)->next;
	}
	
	for (i = 0; i < llsp->columns - 1; i++)
		*(llsp->learn->cur++) = metrics[i];
	*(llsp->learn->cur++) = time;
}

const double *llsp_finalize(llsp_t *llsp)
{
	double *result = NULL;
	
	if (llsp->learn) {
		/* we traverse the list to correct the last row count */
		metrics_node_t *node;
		
		llsp->learn->total_rows = 0;
		for (node = llsp->learn->head; node->next; node = node->next)
			llsp->learn->total_rows += node->rows;
		/* the last node might not be fully filled -> correct that */
		node->rows = (llsp->learn->cur - node->metrics) / llsp->columns;
		llsp->learn->total_rows += node->rows;
		/* keep consistent */
		llsp->learn->last = llsp->learn->cur;
		
		if (llsp_solve(llsp)) result = llsp->result;
	}
	
	return result;
}

double llsp_predict(const llsp_t *llsp, const double *metrics)
{
	unsigned i;
	double result = 0.0;
	for (i = 0; i < llsp->columns - 1; i++)
		result += llsp->result[i] * metrics[i];
	return result;
}

const double *llsp_load(llsp_t *llsp, const char *filename)
{
	double *result = NULL;
	int fd = -1;
	unsigned i;
	
	do {
		off_t offset;
		
		fd = open(filename, O_RDONLY);
		if (fd < 0) {
			printf("opening error, coefficients could not be loaded\n");
			break;
		}
		
		offset = MAX_METRICS * llsp->id * sizeof(double);
		if (lseek(fd, offset, SEEK_SET) != offset) {
			printf("seeking error, coefficients could not be loaded\n");
			break;
		}
		
		for (i = 0; i < llsp->columns - 1; i++) {
			double value;
			if (read(fd, &value, sizeof(value)) != sizeof(value)) {
				printf("reading error, coefficients could not be loaded\n");
				break;
			}
			llsp->result[i] = value;
		}
		if (i == llsp->columns - 1) result = llsp->result;
	} while (0);
	
	if (fd >= 0) close(fd);
	
	return result;
}

int llsp_store(const llsp_t *llsp, const char *filename)
{
	int result = 0;
	int fd = -1;
	unsigned i;
	
	do {
		off_t offset;
		
		fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd < 0) {
			printf("opening error, coefficients could not be stored\n");
			break;
		}
		
		offset = MAX_METRICS * llsp->id * sizeof(double);
		if (lseek(fd, offset, SEEK_SET) != offset) {
			printf("seeking error, coefficients could not be stored\n");
			break;
		}
		
		for (i = 0; i < MAX_METRICS; i++) {
			double value;
			if (i < llsp->columns - 1)
				value = llsp->result[i];
			else
				value = 0.0;
			if (write(fd, &value, sizeof(value)) != sizeof(value)) {
				printf("writing error, coefficients could not be stored\n");
				break;
			}
		}
		if (i == MAX_METRICS) result = 1;
	} while (0);
	
	if (fd >= 0) close(fd);
	
	if (result) {
		printf("prediction coefficients follow:\n");
		for (i = 0; i < llsp->columns - 1; i++)
			printf("\t%G\n", llsp->result[i]);
		return 1;
	} else
		return 0;
}

void llsp_dispose(llsp_t *llsp)
{
	if (llsp->learn) llsp_dispose_learn(llsp);
	free(llsp);
}


static int llsp_solve(llsp_t *llsp)
{
	matrix_t *matrix;
	unsigned long long used_columns;
	char negative_before, negative_after;
	double residue_before, residue_after;
	unsigned row, col, drop_col, result_row;
	
	if (llsp->learn->total_rows < llsp->columns) return 0;
	
	/* Which metric columns are being used? (The time column is always used.) */
	/* Attention: since (llsp->columns - 1) can be as large as the number of bits in
	 * used_columns, this line assumes two's complement for negative values to
	 * work correctly in this corner case; this assumption is believed to be safe */
	used_columns = ((unsigned long long)1 << (llsp->columns - 1)) - 1;
	
	while (1) {
		/* at first, we calculate a preliminary result and use that to determine,
		 * if we need to drop bad columns, which potentially make the calculation unstable */
		
		matrix = llsp_copy_to_matrix(llsp, used_columns);
		if (!matrix) return 0;
		
		/* we use pivoting here to get a better result; the coefficients will be
		 * permutated, but the residual sum of squares (called "residue" in variable
		 * names for brevity) will stay the same */
		matrix_qr_decompose(matrix, 1);
		matrix_trisolve(matrix);
		
		/* check for negative coefficients in the result */
		negative_before = 0;
		for (row = 0; row < matrix->columns - 1; row++)
			negative_before += (matrix->value[matrix->columns - 1][row] < 0.0);
		/* if there are no negative coefficients, we eliminate all columns with only
		 * little influence (less than 0.01 percent) on the final prediction results */
		residue_before = fabs(matrix->value[matrix->columns - 1][matrix->columns - 1]) * 1.0001;
		
		matrix_dispose(matrix);
		
		/* now try dropping every column and check, which (if any) should be dropped */
		drop_col = MAX_METRICS + 1;
		for (col = 0; col < llsp->columns - 1; col++) {
			if ((used_columns & ~((unsigned long long)1 << col)) == used_columns)
			/* this column is already dropped, nothing to do */
				continue;
			if ((used_columns & ~((unsigned long long)1 << col)) == 0)
			/* only one column left -> terminate */
				break;
			matrix = llsp_copy_to_matrix(llsp, (used_columns & ~((unsigned long long)1 << col)));
			if (!matrix) return 0;
			matrix_qr_decompose(matrix, 1);
			matrix_trisolve(matrix);
			negative_after = 0;
			for (row = 0; row < matrix->columns - 1; row++)
				negative_after += (matrix->value[matrix->columns - 1][row] < 0.0);
			residue_after = fabs(matrix->value[matrix->columns - 1][matrix->columns - 1]);
			if ((negative_after < negative_before) ||
				(negative_after == negative_before && residue_after < residue_before)) {
				negative_before = negative_after;
				residue_before  = residue_after;
				drop_col = col;
			}
			matrix_dispose(matrix);
		}
		if (drop_col > MAX_METRICS)
			break;
		else
			used_columns &= ~((unsigned long long)1 << drop_col);
	}
	
	/* now we can calculate the final result */
	matrix = llsp_copy_to_matrix(llsp, used_columns);
	if (!matrix) return 0;
	
	matrix_qr_decompose(matrix, 0);
	matrix_trisolve(matrix);
	
	for (result_row = 0, row = 0; result_row < llsp->columns - 1; result_row++, used_columns >>= 1)
		if (used_columns & 1)
			llsp->result[result_row] = matrix->value[matrix->columns - 1][row++];
		else
			llsp->result[result_row] = 0.0;
	
	matrix_dispose(matrix);
	
	return 1;
}

static matrix_t *llsp_copy_to_matrix(llsp_t *llsp, const unsigned long long used_columns)
{
	matrix_t *matrix;
	metrics_node_t *node;
	unsigned row, col, columns;
	
	for (col = 0, columns = 1; col < (sizeof(used_columns) * 8); col++)
		columns += !!(used_columns & ((unsigned long long)1 << col));
	
	matrix = (matrix_t *)malloc(sizeof(matrix_t) + (columns - 1) * sizeof(double *));
	if (!matrix) return NULL;
	
	matrix->rows    = llsp->learn->total_rows;
	matrix->columns = columns;
	
	for (col = 0; col < matrix->columns; col++) {
		matrix->value[col] = (double *)malloc(sizeof(double) * matrix->rows);
		if (!matrix->value[col]) {
			for (col--; col >= 0; col--) free(matrix->value[col]);
			free(matrix);
			return NULL;
		}
	}
	
	for (node = llsp->learn->head, row = col = 0; node; node = node->next) {
		double *cur;
		unsigned i;
		for (i = 0, cur = node->metrics; i < node->rows * llsp->columns; i++, cur++) {
			/* check, if this is the time column or a used metrics column */
			if (i % llsp->columns == llsp->columns - 1 || 
				used_columns & ((unsigned long long)1 << (i % llsp->columns))) {
				matrix->value[col][row] = *cur;
				if (++col == matrix->columns) {
					col = 0;
					row++;
				}
			}
		}
	}
	
	return matrix;
}

static void matrix_qr_decompose(matrix_t *matrix, char pivoting)
{
	unsigned step;
	
	/* QR decompostion using householder transformation */
	for (step = 0; step < matrix->columns; step++) {
		double *col1;
		long double sigma, s;
		unsigned rows, row, cols, col;
		
		rows = matrix->rows - step;
		cols = matrix->columns - step;
		
		if (pivoting) {
			/* we optionally use column pivoting to further stengthen the numeric stability,
			 * especially for rank deficient matrices: select the unreduced column with the
			 * largest norm and swap it with the leading unreduced column; of course this
			 * will give permutated coefficients, so don't use it for the final result */
			double norm, max_norm;
			unsigned max_norm_col;
			max_norm = 0.0;
			max_norm_col = 0;
			for (col = 0; col < cols - 1; col++) {
				norm = scalarprod(&matrix->value[col + step][step], &matrix->value[col + step][step], rows);
				if (norm > max_norm) {
					max_norm     = norm;
					max_norm_col = col;
				}
			}
			if (max_norm_col != 0) {
				double *swap;
				swap = matrix->value[step];
				matrix->value[step] = matrix->value[max_norm_col + step];
				matrix->value[max_norm_col + step] = swap;
			}
		}
		
		col1 = &matrix->value[step][step];
		sigma = ((col1[0] >= 0.0) ? -1.0 : 1.0) * sqrt(scalarprod(col1, col1, rows));
		if (sigma == 0.0) continue;
		col1[0] -= sigma;
		
		for (col = 1; col < cols; col++) {
			s = scalarprod(col1, &matrix->value[col + step][step], rows) / (sigma * (long double)col1[0]);
			for (row = 0; row < rows; row++)
				matrix->value[col + step][row + step] += s * (long double)col1[row];
		}
		
#if 0
		/* for better readability of dumps, we could really fill the subdiagonal elements
		 * with zeroes, but since we do not use them any more, we might as well leave them alone */
		for (row = 0; row < rows; row++)
			col1[row] = (row == 0) ? sigma : 0;
#else
		col1[0] = sigma;
#endif
	}
}

static long double scalarprod(double *a, double *b, unsigned rows)
{
	unsigned row;
	long double result;
	for (result = 0.0, row = 0; row < rows; row++)
		result += a[row] * b[row];
	return result;
}

static void matrix_trisolve(matrix_t *matrix)
{
	double *last_col;
	int row, col;
	
	last_col = matrix->value[matrix->columns - 1];
	
	for (row = matrix->columns - 2; row >= 0; row--) {
		if (matrix->value[row][row] != 0.0) {
			for (col = matrix->columns - 2; col > row; col--)
				last_col[row] -= matrix->value[col][row] * last_col[col];
			last_col[row] /= matrix->value[row][row];
		} else
			last_col[row] = 0.0;
	}
}

static void matrix_dispose(matrix_t *matrix)
{
	unsigned col;
	for (col = 0; col < matrix->columns; col++)
		free(matrix->value[col]);
	free(matrix);
}

static void llsp_dispose_learn(llsp_t *llsp)
{
	metrics_node_t *node, *next;
	for (node = llsp->learn->head; node; node = next) {
		next = node->next;
		free(node);
	}
	free(llsp->learn);
	llsp->learn = NULL;
}


static void llsp_dump(llsp_t *llsp)
{
	metrics_node_t *node;
	for (node = llsp->learn->head; node; node = node->next) {
		double *cur;
		unsigned i;
		for (i = 0, cur = node->metrics; i < node->rows * llsp->columns; i++, cur++) {
			if (i % llsp->columns < llsp->columns - 1)
				printf("%G, ", *cur);
			else
				printf("%G\n", *cur);
		}
	}
	printf("-----------------------------------------------------------\n");
}

static void matrix_dump(matrix_t *matrix)
{
	unsigned row, col;
	for (row = 0; row < matrix->rows; row++) {
		for (col = 0; col < matrix->columns; col++) {
			if (col < matrix->columns - 1)
				printf("%G, ", matrix->value[col][row]);
			else
				printf("%G\n", matrix->value[col][row]);
		}
	}
	printf("-----------------------------------------------------------\n");
}

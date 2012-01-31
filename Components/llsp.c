/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "llsp.h"

/* float values below this are considered to be 0 */
#define EPSILON 1E-10


struct matrix_s {
	double    *data;         // pointer to the malloc'ed data block, matrix is transposed
	double   **full;         // pointers to the individual columns for easy column dropping
	unsigned   full_cols;    // column count of all columns
	double   **drop;         // matrix with some columns dropped
	unsigned   drop_cols;    // remaining column count after dropping
};

struct llsp_s {
	unsigned         metrics;
	float            drop_threshold;
	struct matrix_s  matrix;           // the running solutions matrix, remember: transposed
	struct matrix_s *scratch;          // pre-allocated scratchpad matrices for column dropping
	double           last_prediction;
	double           result[];         // the resulting coefficients
};

static inline void givens_rotate(struct matrix_s *matrix, unsigned row, unsigned column);
static inline struct matrix_s *stabilize(struct matrix_s *matrix, struct matrix_s *scratch, double residue_bound, unsigned drop_start);
static inline void trisolve(struct matrix_s *matrix);
#ifdef __GNUC__
static void dump_matrix(double **matrix, unsigned size) __attribute__((unused));
#else
static void dump_matrix(double **matrix, unsigned size);
#endif


#pragma mark -
#pragma mark LLSP API Functions

llsp_t *llsp_new(unsigned count)
{
	llsp_t *llsp;
	
	if (count < 1) return NULL;
	
	llsp = malloc(sizeof(llsp_t) + count * sizeof(double));
	if (!llsp) return NULL;
	
	llsp->metrics = count;
	llsp->drop_threshold = COLUMN_DROP_THRESHOLD_START;
	llsp->matrix.data = NULL;
	llsp->matrix.full = NULL;
	llsp->matrix.full_cols = count + 1;
	llsp->matrix.drop = NULL;
	llsp->matrix.drop_cols = 0;
	llsp->scratch = NULL;
	llsp->last_prediction = 0.0;
	
	return llsp;
}

void llsp_add(llsp_t *llsp, const double *metrics, double time)
{
	const unsigned column_count = llsp->matrix.full_cols;
	const unsigned row_count = llsp->matrix.full_cols + 1;  // one extra row for shifting down
	const size_t matrix_size = column_count * sizeof(double *);
	const size_t data_size = column_count * row_count * sizeof(double);
	
	if (!llsp->matrix.data) {
		llsp->matrix.data = malloc(data_size);
		if (!llsp->matrix.data) goto error;
		memset(llsp->matrix.data, 0, data_size);
		
		llsp->matrix.full = malloc(matrix_size);
		if (!llsp->matrix.full) goto error;
		for (unsigned column = 0; column < llsp->matrix.full_cols; column++)
			llsp->matrix.full[column] = llsp->matrix.data + column * row_count;
		
		llsp->matrix.drop = malloc(matrix_size);
		if (!llsp->matrix.drop) goto error;
		memset(llsp->matrix.drop, 0, matrix_size);
		
		llsp->scratch = malloc(llsp->metrics * sizeof(llsp->scratch[0]));
		if (!llsp->scratch) goto error;
		
		llsp->scratch[0].data = malloc(llsp->metrics * data_size);
		llsp->scratch[0].full = malloc(llsp->metrics * matrix_size);
		llsp->scratch[0].drop = malloc(llsp->metrics * matrix_size);
		if (!llsp->scratch[0].data) goto error;
		if (!llsp->scratch[0].full) goto error;
		if (!llsp->scratch[0].drop) goto error;
		for (unsigned depth = 0; depth < llsp->metrics; depth++) {
			llsp->scratch[depth].data = llsp->scratch[0].data + depth * column_count * row_count;
			llsp->scratch[depth].full = llsp->scratch[0].full + depth * column_count;
			llsp->scratch[depth].drop = llsp->scratch[0].drop + depth * column_count;
			llsp->scratch[depth].full_cols = llsp->matrix.full_cols;
			for (unsigned column = 0; column < llsp->scratch[depth].full_cols; column++)
				llsp->scratch[depth].full[column] = llsp->scratch[depth].data + column * row_count;
		}
	}
	
	/* age out the past a little bit */
	for (unsigned element = 0; element < row_count * column_count; element++)
		llsp->matrix.data[element] *= AGING_FACTOR;
	
	/* add new row to the top of the solving matrix */
	memmove(llsp->matrix.data + 1, llsp->matrix.data, data_size - sizeof(double));
	for (unsigned column = 0; column < llsp->metrics; column++)
		llsp->matrix.full[column][0] = metrics[column];
	llsp->matrix.full[llsp->metrics][0] = time;
	
	/* create a drop-column-matrix without the all-zero columns */
	unsigned drop_column = 0;
	for (unsigned column = 0; column < llsp->metrics; column++) {
		if (llsp->matrix.drop[drop_column] == llsp->matrix.full[column]) {
			// always include all columns in use before
			drop_column++;
			continue;
		}
		for (unsigned row = 0; row <= column + 1; row++) {
			if (llsp->matrix.full[column][row] >= EPSILON) {
				llsp->matrix.drop[drop_column++] = llsp->matrix.full[column];
				break;
			}
		}
	}
	// last column with the times always gets in
	llsp->matrix.drop[drop_column++] = llsp->matrix.full[llsp->metrics];
	memset(llsp->matrix.drop + drop_column, 0, matrix_size - drop_column * sizeof(double *));
	llsp->matrix.drop_cols = drop_column;
	
	for (unsigned i = 0; i < llsp->matrix.drop_cols; i++)
		givens_rotate(&llsp->matrix, i + 1, i);
	
	return;
	
error:
	free(llsp->matrix.data);
	free(llsp->matrix.full);
	free(llsp->matrix.drop);
	
	if (llsp->scratch) {
		free(llsp->scratch[0].data);
		free(llsp->scratch[0].full);
		free(llsp->scratch[0].drop);
		free(llsp->scratch);
	}
}

const double *llsp_solve(llsp_t *llsp)
{
	double *result = NULL;
	
	if (llsp->matrix.data) {
		trisolve(&llsp->matrix);
		
		double residue_bound = fabs(llsp->matrix.drop[llsp->matrix.drop_cols - 1][llsp->matrix.drop_cols - 1]) * llsp->drop_threshold;
		struct matrix_s *solution = stabilize(&llsp->matrix, llsp->scratch, residue_bound, 0);
		
		if (llsp->drop_threshold > COLUMN_DROP_THRESHOLD_END)
			llsp->drop_threshold -= COLUMN_DROP_THRESHOLD_STEP;
		
		unsigned result_row = solution->full_cols;
		for (unsigned column = 0; column < llsp->metrics; column++)
			llsp->result[column] = solution->full[column][result_row];
		
		result = llsp->result;
	}
	
	return result;
}

double llsp_predict(llsp_t *llsp, const double *metrics)
{
	double result = 0.0;
	for (unsigned i = 0; i < llsp->metrics; i++)
		result += llsp->result[i] * metrics[i];
	
	if (result >= 0.0)
		llsp->last_prediction = result;
	else
		result = llsp->last_prediction;
	
	return result;
}

void llsp_dispose(llsp_t *llsp)
{
	free(llsp->matrix.data);
	free(llsp->matrix.full);
	free(llsp->matrix.drop);
	
	if (llsp->scratch) {
		free(llsp->scratch[0].data);
		free(llsp->scratch[0].full);
		free(llsp->scratch[0].drop);
		free(llsp->scratch);
	}
	
	free(llsp);
}


#pragma mark -
#pragma mark Helper Functions

static void givens_rotate(struct matrix_s *matrix, unsigned row, unsigned column)
{
	if (fabs(matrix->drop[column][row]) < EPSILON)
		return;  // already zero
	
	const unsigned i = row;
	const unsigned j = column;
	const double a_ij = matrix->drop[j][i];
	const double a_jj = matrix->drop[j][j];
	const double rho = ((a_jj < 0.0) ? -1.0 : 1.0) * sqrt(a_jj * a_jj + a_ij * a_ij);
	const double c = a_jj / rho;
	const double s = a_ij / rho;
	
	for (unsigned x = 0; x < matrix->drop_cols; x++) {
		const double a_ix = matrix->drop[x][i];
		const double a_jx = matrix->drop[x][j];
		matrix->drop[x][i] = c * a_ix - s * a_jx;
		matrix->drop[x][j] = c * a_jx + s * a_ix;
	}
}

static inline struct matrix_s *stabilize(struct matrix_s *matrix, struct matrix_s *scratch, double residue_bound, unsigned drop_start)
{
	struct matrix_s *result = matrix;
	
	const unsigned column_count = matrix->full_cols;
	const unsigned row_count = matrix->full_cols + 1;
	const size_t data_size = column_count * row_count * sizeof(double);
	
	for (unsigned drop = drop_start; drop < matrix->drop_cols - 1; drop++) {
		/* copy current matrix to scratchpad */
		memcpy(scratch->data, matrix->data, data_size);
		unsigned drop_column = 0;
		for (unsigned column = 0; column < matrix->full_cols; column++)
			if (matrix->full[column] == matrix->drop[drop_column])
				scratch->drop[drop_column++] = scratch->full[column];
		scratch->drop_cols = drop_column;
		
		/* drop column and givens fixup */
		memmove(scratch->drop + drop, scratch->drop + drop + 1, (scratch->drop_cols - drop - 1) * sizeof(double *));
		scratch->drop_cols--;
		for (unsigned fix = drop; fix < scratch->drop_cols; fix++)
			givens_rotate(scratch, fix + 1, fix);
		
		trisolve(scratch);
		
		/* evaluate solution */
		unsigned result_row = matrix->full_cols;
		unsigned new_negative = 0, old_negative = 0;
		for (unsigned column = 0; column < matrix->full_cols - 1; column++) {
			if (scratch->full[column][result_row] < 0.0) new_negative++;
			if (result->full[column][result_row] < 0.0) old_negative++;
		}
		
		double new_residue = fabs(scratch->drop[scratch->drop_cols - 1][scratch->drop_cols - 1]);
		double old_residue = fabs(result->drop[result->drop_cols - 1][result->drop_cols - 1]);
		
		if (new_negative < old_negative ||
		    (new_negative == old_negative && new_residue < old_residue) ||
		    (new_residue < residue_bound && new_residue > old_residue)) {
			result = stabilize(scratch, scratch + 1, residue_bound, drop);
			if (result == scratch) scratch++;  // use a new scratch so we don't overwrite
		}
	}
	
	return result;
}

static inline void trisolve(struct matrix_s *matrix)
{
	unsigned result_row = matrix->full_cols;  // use extra row to solve the coefficients
	for (unsigned column = 0; column < matrix->full_cols - 1; column++)
		matrix->full[column][result_row] = 0.0;
	for (int row = matrix->drop_cols - 2; row >= 0; row--) {
		unsigned column = row;
		if (fabs(matrix->drop[column][row]) >= EPSILON) {
			column = matrix->drop_cols - 1;
			double intermediate = matrix->drop[column][row];
			for (column--; column > row; column--)
				intermediate -= matrix->drop[column][result_row] * matrix->drop[column][row];
			matrix->drop[column][result_row] = intermediate / matrix->drop[column][row];
		}
	}
}

static void dump_matrix(double **matrix, unsigned size)
{
	for (unsigned row = 0; row < size; row++) {
		for (unsigned column = 0; column < size; column++)
			if (fabs(matrix[column][row]) < EPSILON)
				printf("       0 ");
			else
				printf("%+8.2lG ", matrix[column][row]);
		printf("\n");
	}
}

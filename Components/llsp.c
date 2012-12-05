/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "llsp.h"

/* float values below this are considered to be 0 */
#define EPSILON 1E-10


struct stats {
	double average;          // running expected value (first moment)
	double squares;          // running variance (second moment)
	double product;          // running expected product of value and execution time
};

struct matrix {
	double **matrix;         // matrix data, indexed columns first, rows second
	size_t   columns;        // column count
};

struct llsp_s {
	size_t        metrics;   // metrics count
	double       *data;      // pointer to the malloc'ed data block, matrix is transposed
	struct matrix full;      // pointers to the individual columns for easy column dropping
	struct matrix sort;      // matrix columns sorted by correlation
	struct matrix good;      // reduced matrix with low-contribution columns dropped
	struct stats *stats;     // statistics for sorting
	size_t        age;       // how much training tuples the solver received
	double        last_prediction;
	double        result[];  // the resulting coefficients
};

static void givens_fixup(struct matrix m, size_t row, size_t column);
static void stabilize(struct matrix in, struct matrix *out);
static void trisolve(struct matrix m);

#pragma mark -


#pragma mark LLSP API Functions

llsp_t *llsp_new(size_t count)
{
	llsp_t *llsp;
	
	if (count < 1) return NULL;
	
	size_t llsp_size = sizeof(llsp_t) + count * sizeof(double);  // extra room for coefficients
	llsp = malloc(llsp_size);
	if (!llsp) return NULL;
	memset(llsp, 0, llsp_size);
	
	llsp->metrics = count;
	llsp->full.columns = count + 1;
	
	return llsp;
}

void llsp_add(llsp_t *llsp, const double *metrics, double time)
{
	const size_t column_count = llsp->full.columns;
	const size_t row_count = llsp->full.columns + 1;  // one extra row for shifting down
	const size_t data_size = column_count * row_count * sizeof(double);
	const size_t matrix_size = column_count * sizeof(double *);
	const size_t stats_size = column_count * sizeof(struct stats);
	
	if (!llsp->data) {
		llsp->data        = malloc(2 * data_size);
		llsp->full.matrix = malloc(3 * matrix_size);
		llsp->stats       = malloc(stats_size);
		if (!llsp->data || !llsp->full.matrix || !llsp->stats)
			abort();
		
		llsp->sort.matrix = llsp->full.matrix + column_count;
		llsp->good.matrix = llsp->sort.matrix + column_count;
		for (size_t column = 0; column < llsp->full.columns; column++) {
			llsp->full.matrix[column] = llsp->data + column * row_count;
		}
		llsp->good.matrix[0] = llsp->data + column_count * row_count;
		
		memset(llsp->data, 0, data_size);
		memset(llsp->sort.matrix, 0, matrix_size);
		memset(llsp->stats, 0, stats_size);
	}
	
	/* age out the past a little bit */
	for (size_t element = 0; element < row_count * column_count; element++)
		llsp->data[element] *= AGING_FACTOR;
	
	/* add new row to the top of the solving matrix */
	memmove(llsp->data + 1, llsp->data, data_size - sizeof(double));
	for (size_t column = 0; column < llsp->metrics; column++)
		llsp->full.matrix[column][0] = metrics[column];
	llsp->full.matrix[llsp->metrics][0] = time;
	
	/* collect all non-zero metrics in sort matrix */
	size_t last_sort_column = llsp->sort.columns ? llsp->sort.columns - 1 : 0;
	for (size_t column = 0; column < llsp->metrics; column++) {
		if (fabs(llsp->full.matrix[column][0]) >= EPSILON) {
			size_t compare;
			for (compare = 0; compare < llsp->sort.columns; compare++)
				if (llsp->full.matrix[column] == llsp->sort.matrix[compare])
					break;
			if (compare == llsp->sort.columns)
				// this metric appears for the first time, add column at the end
				llsp->sort.matrix[last_sort_column++] = llsp->full.matrix[column];
		} else
			llsp->full.matrix[column][0] = 0.0;
	}
	// last column with the times always gets in last
	llsp->sort.matrix[last_sort_column++] = llsp->full.matrix[llsp->metrics];
	// clear empty columns to the right
	memset(llsp->sort.matrix + last_sort_column, 0, matrix_size - last_sort_column * sizeof(double *));
	llsp->sort.columns = last_sort_column;
	
	/* update statistics for sorting */
	for (size_t column = 0; column < llsp->sort.columns; column++) {
		const double new_value = llsp->sort.matrix[column][0];
		const double old_weight = llsp->age / (llsp->age + 1.0);
		const double new_weight = 1.0 / (llsp->age + 1.0);
		llsp->stats[column].average *= old_weight;
		llsp->stats[column].average += new_weight * new_value;
		llsp->stats[column].squares *= old_weight;
		llsp->stats[column].squares += new_weight * new_value * new_value;
		llsp->stats[column].product *= old_weight;
		llsp->stats[column].product += new_weight * new_value * time;
	}
	if (llsp->age / (llsp->age + 1.0) < AGING_FACTOR)
		llsp->age++;  // stops incrementing after a while, sliding window from there
	
	/* givens fixup of the subdiagonal */
	for (size_t i = 0; i < llsp->sort.columns; i++)
		givens_fixup(llsp->sort, i + 1, i);
	
	/* a single bubble sort step to push badly correlating metrics to the right */
	double previous_pearson = INFINITY;
	for (size_t column = 0; column < llsp->sort.columns - 1; column++) {
		const struct stats *x = &llsp->stats[column];
		const struct stats *y = &llsp->stats[llsp->sort.columns - 1];
		const double covariance = x->product - x->average * y->average;
		const double variance_x = x->squares - x->average * x->average;
		const double variance_y = y->squares - y->average * y->average;
		const double divisor = sqrt(variance_x * variance_y);
		const double pearson_correlation = (divisor >= EPSILON ? (covariance / divisor) : 0.0);
		
		if (pearson_correlation > previous_pearson) {
			// swap with previous column
			double *matrix_tmp = llsp->sort.matrix[column - 1];
			llsp->sort.matrix[column - 1] = llsp->sort.matrix[column];
			llsp->sort.matrix[column] = matrix_tmp;
			struct stats stats_tmp = llsp->stats[column - 1];
			llsp->stats[column - 1] = llsp->stats[column];
			llsp->stats[column] = stats_tmp;
			givens_fixup(llsp->sort, column, column - 1);
		}

		previous_pearson = pearson_correlation;
	}
}

const double *llsp_solve(llsp_t *llsp)
{
	double *result = NULL;
	
	if (llsp->data) {
//		stabilize(llsp->sort, &llsp->good);
//		trisolve(llsp->good);
		trisolve(llsp->sort);
		
		size_t result_row = llsp->sort.columns;
		for (size_t column = 0; column < llsp->metrics; column++)
			llsp->result[column] = llsp->full.matrix[column][result_row];
		result = llsp->result;
	}
	
	return result;
}

double llsp_predict(llsp_t *llsp, const double *metrics)
{
	double result = 0.0;
	for (size_t i = 0; i < llsp->metrics; i++)
		result += llsp->result[i] * metrics[i];
	
	if (result >= 0.0)
		llsp->last_prediction = result;
	else
		result = llsp->last_prediction;
	
	return result;
}

void llsp_dispose(llsp_t *llsp)
{
	free(llsp->data);
	free(llsp->full.matrix);
	free(llsp->stats);
	free(llsp);
}

#pragma mark -


#pragma mark Helper Functions

static void givens_fixup(struct matrix m, size_t row, size_t column)
{
	if (fabs(m.matrix[column][row]) < EPSILON) {
		// alread zero
		m.matrix[column][row] = 0.0;
		return;
	}
	
	const size_t i = row;
	const size_t j = column;
	const double a_ij = m.matrix[j][i];
	const double a_jj = m.matrix[j][j];
	const double rho = ((a_jj < 0.0) ? -1.0 : 1.0) * sqrt(a_jj * a_jj + a_ij * a_ij);
	const double c = a_jj / rho;
	const double s = a_ij / rho;
	
	for (size_t x = column; x < m.columns; x++) {
		if (x == column) {
			// the real calculation below should produce the same, but this is more stable
			m.matrix[x][i] = 0.0;
			m.matrix[x][j] = rho;
		} else {
			const double a_ix = m.matrix[x][i];
			const double a_jx = m.matrix[x][j];	
			m.matrix[x][i] = c * a_ix - s * a_jx;
			m.matrix[x][j] = s * a_ix + c * a_jx;
		}
		
		if (fabs(m.matrix[x][i]) < EPSILON)
			m.matrix[x][i] = 0.0;
		if (fabs(m.matrix[x][j]) < EPSILON)
			m.matrix[x][j] = 0.0;
	}
}

static void stabilize(struct matrix in, struct matrix *out)
{
	const size_t column_count = in.columns;
	const size_t row_count = in.columns + 1;  // one extra row for trisolve results
	const size_t column_size = row_count * sizeof(double);
	const size_t matrix_end = column_count - 1;
	
	/* copy complete matrix to target */
	out->columns = in.columns;
	for (size_t column = 0; column < column_count; column++) {
		out->matrix[column] = out->matrix[0] + column * row_count;
		memcpy(out->matrix[column], in.matrix[column], column_size);
	}
	
	/* try dropping all columns and watch the residual error */
	bool drop[matrix_end];
	drop[0] = false;  // first column is never dropped, with sorted input this is the best correlating column
	double previous_residual = fabs(out->matrix[matrix_end][matrix_end]);
	for (size_t column = matrix_end - 1; column > 0; column--) {
		// drop column
		out->columns--;
		out->matrix[column] = out->matrix[column + 1];
		
		givens_fixup(*out, column + 1, column);
		
		double residual = fabs(out->matrix[column][column]);
		drop[column] = (residual / previous_residual < COLUMN_CONTRIBUTION);
		previous_residual = residual;
	}
	
	/* back to complete matrix again */
	out->columns = in.columns;
	for (size_t column = 0; column < column_count; column++) {
		out->matrix[column] = out->matrix[0] + column * row_count;
		memcpy(out->matrix[column], in.matrix[column], column_size);
	}
	
	/* iteratively drop columns marked before */
	for (size_t column = matrix_end - 1; column > 0; column--) {
		if (drop[column]) {
			out->columns--;
			memmove(&out->matrix[column], &out->matrix[column + 1], (out->columns - column) * sizeof(double *));
			for (size_t fix_column = column; column < out->columns; column++)
				givens_fixup(*out, fix_column + 1, fix_column);
		}
	}
}

static void trisolve(struct matrix m)
{
	size_t result_row = m.columns;  // use extra row to solve the coefficients
	for (size_t column = 0; column < m.columns - 1; column++)
		m.matrix[column][result_row] = 0.0;
	
	for (ssize_t row = result_row - 2; row >= 0; row--) {
		ssize_t column = row;
		
		if (fabs(m.matrix[column][row]) >= EPSILON) {
			column = m.columns - 1;
			
			double intermediate = m.matrix[column][row];
			for (column--; column > row; column--)
				intermediate -= m.matrix[column][result_row] * m.matrix[column][row];
			m.matrix[column][result_row] = intermediate / m.matrix[column][row];

			for (column--; column >= 0; column--)
				assert(m.matrix[column][row] == 0.0);  // check for upper triangular matrix
		} else
			m.matrix[column][row] = 0.0;
	}
}

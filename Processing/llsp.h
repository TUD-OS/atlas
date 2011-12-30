/*
 * Copyright (C) 2006-2011 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

/* The running solution can be made to age out previously acquired knowledge
 * over time. When this aging factor is set to 1.0, no aging is performed. This
 * means the solution will be equivalent to a LLS-solution over all previous
 * metrics and decoding times. With an aging factor a little lower than one,
 * the solution will slightly lean towards newly added metrics/times, exhibting
 * properties of a sliding average. */
#define AGING_FACTOR 0.999L
 
/* Columns with only little influence on the result are dropped to
 * prevent overfitting. The threshold specifies the acceptable increase
 * of the residual error for a column to be dropped. Increase this
 * value to improve stability at the cost of accuracy. */
#define COLUMN_DROP_THRESHOLD_START 2.0
#define COLUMN_DROP_THRESHOLD_STEP 0.01
#define COLUMN_DROP_THRESHOLD_END 1.01

/* an opaque handle for the LLSP solver/predictor */
typedef struct llsp_s llsp_t;

/* Allocated a new LLSP handle with the given number of metrics. */
llsp_t *llsp_new(unsigned count);

/* This function adds another tuple of (metrics, measured decoder time) to the
 * LLSP solution. The metrics array must have as many values as stated in count
 * on llsp_new(). */
void llsp_add(llsp_t *llsp, const double *metrics, double time);

/* Solves the LLSP and returns a pointer to the resulting coefficients or NULL
 * if the training phase could not be successfully finalized. The pointer
 * remains valid until the LLSP context is freed. */
const double *llsp_solve(llsp_t *llsp);

/* Predicts the decoding time from the given metrics. The context has to be
 * populated with a set of prediction coefficients by running llsp_solve(). */
double llsp_predict(llsp_t *llsp, const double *metrics);

/* Frees the LLSP context. */
void llsp_dispose(llsp_t *llsp);

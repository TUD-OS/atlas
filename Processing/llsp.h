/*
 * Copyright (C) 2006-2011 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

/* 
 * The operation of the llsp decoding time predictor is divided in two
 * phases: First, the predictor has to be trained with a mix of typical
 * videos. After that, predicted decoding times can be retrieved. The
 * same llsp handle can be used for both, even in parallel (that is:
 * predicting decoding times with previous coefficients and accumulating
 * samples for calculating new coefficients at the same time).
 */

/* an opaque handle for the llsp solver/predictor */
typedef struct llsp_s llsp_t;

/* Allocated a new llsp handle with the given id. The id is meant to
 * distinguish several prediction contexts and is typically used to
 * handle the different codecs or different frame types (I,P,B,S,...)
 * within the same codec, so use some unique codec/frame type number.
 * Count denotes the number of metrics values used in this context. */
llsp_t *llsp_new(int id, unsigned count);

/* This function is called during the training phase and adds another
 * tuple of (metrics, measured decoder time) to the training matrix
 * inside the given llsp solver. The metrics array must have as many
 * values as stated in count on llsp_new(). */
void llsp_accumulate(llsp_t *llsp, const double *metrics, double time);

/* Finalizes the training phase for the given context and returns a
 * pointer to the resulting coefficients for inspection by the client
 * or NULL if the training phase could not be successfully finalized.
 * The pointer is valid until the llsp context is freed. */
const double *llsp_finalize(llsp_t *llsp);

/* Predicts the decoding time from the given metrics. The context has
 * to be populated with a set of prediction coefficients, either by
 * running llsp_load() or by a training phase with a successfully
 * finished llsp_finalize(). Otherwise, the resulting prediction is
 * undefined. */
double llsp_predict(const llsp_t *llsp, const double *metrics);

/* Populates the llsp context with previously stored prediction
 * coefficients. Returns a pointer to the coefficients for inspection
 * by the client or NULL, if no stored coefficients for the current
 * id and metrics count have been found. */
const double *llsp_load(llsp_t *llsp, const char *filename);

/* Stores the current prediction coefficients. The context has to be
 * populated by either llsp_load() or by a training phase with a
 * successfully finished llsp_finalize(). Otherwise, the stored
 * coefficients are undefined. */
int llsp_store(const llsp_t *llsp, const char *filename);

/* Clears an llsp context. Functionally equivalent to disposing and
 * reallocating it, but this saves you some memory allocations. */
void llsp_purge(llsp_t *llsp);

/* Frees the llsp context. */
void llsp_dispose(llsp_t *llsp);

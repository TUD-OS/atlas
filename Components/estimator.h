/*
 * Copyright (C) 2006-2015 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>


#pragma mark Job Management Configuration

/* To compensate for misprediction, accounting drift and scheduler overhead,
 * jobs are over-allocated when reporting to the system scheduler. */
#ifndef JOB_OVERALLOCATION
#define JOB_OVERALLOCATION(x)  (x > 0.001) ? (x * 1.025) : (x + 0.000025)
//#define JOB_OVERALLOCATION(x)  (x > 0.001) ? (x * 1.025) : (x + 0.000025) + sqrt(estimator->mse)
#endif

/* toggle job communication to scheduler */
#ifdef JOB_SCHEDULING
#	define JOB_SCHEDULING 1
#else
#	define JOB_SCHEDULING 0
#endif

/* toggle time prediction */
#ifdef LLSP_PREDICT
#	define LLSP_PREDICT 1
#else
#	define LLSP_PREDICT 0
#endif

#ifndef IMPLEMENTS_HOOKS
#	define WEAK_SYMBOL __attribute__((weak))
#else
#	define WEAK_SYMBOL
#endif

#pragma mark -


#ifdef __cplusplus
extern "C" {
#endif

#pragma mark Estimator Primitives

typedef struct {
	double deadline;
	size_t metrics_count;
	const double *metrics;
} atlas_job_t;

/* job management */
void atlas_job_submit(void *code, pid_t tid, atlas_job_t job);
void atlas_job_next(void *code);
void atlas_job_train(void *code);

#pragma mark -


#pragma mark Threads and Time

/* pin thread to CPU */
void atlas_pin_cpu(int cpu);

/* the current time in ATLAS' timebase */
double atlas_now(void);

/* clock for thread progress */
double atlas_progress(void);

#pragma mark -


#pragma mark Hooks for Experiments

/* hooks for evaluation */
extern void hook_job_submit(void *code, double prediction, double reservation, double deadline) WEAK_SYMBOL;
extern void hook_job_release(void *code) WEAK_SYMBOL;
extern void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution) WEAK_SYMBOL;
extern void hook_llsp_result(const double *result, size_t count) WEAK_SYMBOL;

#pragma mark -


#pragma mark Ringbuffer Helpers

#ifndef BUFFER_TYPE
#define BUFFER_TYPE int
#endif

struct buffer {
	size_t size;
	BUFFER_TYPE *read;
	BUFFER_TYPE *write;
	BUFFER_TYPE *ring;
};

static const size_t buffer_size_increment = 1024;

static inline void buffer_init(struct buffer *buffer)
{
	buffer->size = buffer_size_increment;
	buffer->ring = (BUFFER_TYPE *)malloc(buffer->size * sizeof(BUFFER_TYPE));
	if (!buffer->ring) abort();
	buffer->read = buffer->ring;
	buffer->write = buffer->ring;
}

static inline void buffer_put(struct buffer *buffer, BUFFER_TYPE value)
{
	*buffer->write = value;
	if (++buffer->write >= buffer->ring + buffer->size)
		buffer->write = buffer->ring;  // ring buffer wrap around
	if (buffer->write == buffer->read) {  // buffer full
		size_t old_pos = (size_t)(buffer->read - buffer->ring);
		size_t old_end = buffer->size;
		buffer->size += buffer_size_increment;
		buffer->ring = (BUFFER_TYPE *)realloc(buffer->ring, buffer->size * sizeof(BUFFER_TYPE));
		if (!buffer->ring) abort();
		buffer->read = buffer->ring + old_pos;
		buffer->write = buffer->ring + old_pos;
		memmove(buffer->read + buffer_size_increment, buffer->read, old_end - old_pos);
		buffer->read += buffer_size_increment;
	}
}

static inline BUFFER_TYPE buffer_get(struct buffer *buffer)
{
	BUFFER_TYPE value = *buffer->read;
	if (++buffer->read >= buffer->ring + buffer->size)
		buffer->read = buffer->ring;  // ring buffer wrap around
	return value;
}

#ifdef __cplusplus
}
#endif

/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#ifdef __linux__
#define _GNU_SOURCE
#include <sched.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#include "scheduler.h"
#include "llsp.h"
#include "jobs.h"

/* A singly linked list of execution time estimators.
 * We never dequeue nodes from the list, so traversing does not need locking. */
struct estimator_s {
	struct estimator_s *next;
	pthread_mutex_t lock;
	void *code;
	pid_t tid;
	
	double time;
	double previous_deadline;
	unsigned metrics_count;
	llsp_t *llsp;
	
	struct scratchpad {
		unsigned size;
		double *read;
		double *write;
		double ringbuffer[];
	} scratchpad;
};

static const unsigned initial_metrics_size = 256;

static pthread_mutex_t estimator_enqueue = PTHREAD_MUTEX_INITIALIZER;
static struct estimator_s *estimator_list = NULL;

static inline void scratchpad_write(struct scratchpad *scratchpad, double value)
{
	*scratchpad->write = value;
	if (++scratchpad->write > scratchpad->ringbuffer + scratchpad->size)
		scratchpad->write = scratchpad->ringbuffer;  // ring buffer wrap around
	assert(scratchpad->write != scratchpad->read);  // FIXME: realloc when full
}

static inline double scratchpad_read(struct scratchpad *scratchpad)
{
	double value = *scratchpad->read;
	if (++scratchpad->read > scratchpad->ringbuffer + scratchpad->size)
		scratchpad->read = scratchpad->ringbuffer;  // ring buffer wrap around
	return value;
}

#pragma mark -


#pragma mark Job Queue Management

void atlas_job_queue_checkin(void *code)
{
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->code == code) break;
	if (!estimator) {
		estimator = malloc(sizeof(struct estimator_s) + sizeof(double) * initial_metrics_size);
		if (!estimator) abort();
		pthread_mutex_init(&estimator->lock, NULL);
		
		estimator->code = code;
		estimator->tid = 0;
		estimator->time = 0.0;
		estimator->previous_deadline = 0.0;
		estimator->metrics_count = 0;
		estimator->llsp = NULL;
		estimator->scratchpad.size = initial_metrics_size;
		estimator->scratchpad.read = estimator->scratchpad.ringbuffer;
		estimator->scratchpad.write = estimator->scratchpad.ringbuffer;
		
		pthread_mutex_lock(&estimator_enqueue);
		estimator->next = estimator_list;
		estimator_list = estimator;
		pthread_mutex_unlock(&estimator_enqueue);
	}
	
	pthread_mutex_lock(&estimator->lock);
	estimator->tid = gettid();
	pthread_mutex_unlock(&estimator->lock);
	
#ifdef __linux__
	cpu_set_t cpu_set;
	
	CPU_ZERO(&cpu_set);
	CPU_SET(0, &cpu_set);
	
	sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
#endif
}

void atlas_job_queue_terminate(void *code)
{
	struct estimator_s *estimator, *prev;
	for (estimator = estimator_list, prev = NULL; estimator; prev = estimator, estimator = estimator->next)
		if (estimator->code == code) break;
	if (estimator) {
		if (estimator->llsp)
			llsp_dispose(estimator->llsp);
#if 0
		/* We do not dequeue nodes to allow lock-free traversal. Some RCU-style
		 * solution would solve the resuling memory leak. */
		if (prev)
			prev->next = estimator->next;
		else
			estimator_list = estimator->next;
		free(estimator);
#endif
	}
}

#pragma mark -


#pragma mark Job Management

void atlas_job_submit_absolute(void *code, double deadline, unsigned count, const double metrics[])
{
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->code == code) break;
	assert(estimator);
	
	pthread_mutex_lock(&estimator->lock);
	
	if (deadline < estimator->previous_deadline) {
		printf("WARNING: deadlines not ordered (%lf < %lf)\n", deadline, estimator->previous_deadline);
		deadline = estimator->previous_deadline;
	}
	estimator->previous_deadline = deadline;
	
	if (!estimator->llsp) {
		estimator->metrics_count = count;
		estimator->llsp = llsp_new(count);
	}
	assert(estimator->metrics_count == count);
	double prediction = 0.0;
	if (llsp_solve(estimator->llsp))
		prediction = llsp_predict(estimator->llsp, metrics);
	
	for (size_t i = 0; i < estimator->metrics_count; i++)
		scratchpad_write(&estimator->scratchpad, metrics[i]);
	scratchpad_write(&estimator->scratchpad, deadline);
	scratchpad_write(&estimator->scratchpad, prediction);
	
	// FIXME: reasoning about deadline order is impossible when submitting relative deadlines
	struct timeval tv;
	gettimeofday(&tv, NULL);
	double time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
	deadline -= time;
	if (deadline < 0.0) deadline = 0.0;
	
	struct timeval tv_deadline = {
		.tv_sec = deadline,
		.tv_usec = 1000000 * (deadline - (long long)deadline)
	};
	struct timeval tv_exectime = {
		.tv_sec = prediction,
		.tv_usec = 1000000 * (prediction - (long long)prediction)
	};
	sched_submit(estimator->tid, &tv_exectime, &tv_deadline);
	
	pthread_mutex_unlock(&estimator->lock);
}

void atlas_job_submit_relative(void *code, double deadline, unsigned count, const double metrics[])
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	double time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
	
	atlas_job_submit_absolute(code, time + deadline, count, metrics);
}

void atlas_job_next(void *code)
{
	double time;
#ifdef __linux__
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
#warning falling back to gettimeofday() which includes blocking and waiting time, results will be wrong
	struct timeval tv;
	gettimeofday(&tv, NULL);
	time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->code == code) break;
	assert(estimator);
	
	pthread_mutex_lock(&estimator->lock);
	
	if (estimator->scratchpad.read != estimator->scratchpad.write) {
		double metrics[estimator->metrics_count];
		for (size_t i = 0; i < estimator->metrics_count; i++)
			metrics[i] = scratchpad_read(&estimator->scratchpad);
		(void)scratchpad_read(&estimator->scratchpad);  // deadline
		(void)scratchpad_read(&estimator->scratchpad);  // prediction
		if (estimator->time > 0.0)
			llsp_add(estimator->llsp, metrics, time - estimator->time);
	}
	estimator->time = time;
	
	pthread_mutex_unlock(&estimator->lock);
	
	sched_next();
}

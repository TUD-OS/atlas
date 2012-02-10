/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#include "llsp.h"
#include "estimate.h"

struct estimator_s {
	struct estimator_s *next;
	unsigned queue;
	double time;
	llsp_t *llsp;
	unsigned metrics_count;
	unsigned size;
	double *read_pos;
	double *write_pos;
	double metrics[];
};

static const unsigned initial_metrics_size = 256;

static pthread_mutex_t estimator_lock = PTHREAD_MUTEX_INITIALIZER;  // TODO: finegrained locking
static struct estimator_s *estimator_list = NULL;


static struct estimator_s *estimator_alloc(unsigned queue)
{
	struct estimator_s *estimator;
	
	estimator = malloc(sizeof(struct estimator_s) + sizeof(double) * initial_metrics_size);
	estimator->queue = queue;
	estimator->time = 0.0;
	estimator->llsp = NULL;
	estimator->metrics_count = 0;
	estimator->size = initial_metrics_size;
	estimator->read_pos = estimator->metrics;
	estimator->write_pos = estimator->metrics;
	estimator->next = estimator_list;
	estimator_list = estimator;
	return estimator;
}

#pragma mark -


#pragma mark Thread Registration

void thread_checkin(unsigned queue)
{
	pid_t tid;
#ifdef __linux__
	tid = gettid();
#else
	tid = 0;
#endif
	
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->queue == queue) break;
	if (!estimator)
		estimator = estimator_alloc(queue);
	
	pthread_mutex_unlock(&estimator_lock);
	// TODO: notify scheduler
}

void thread_checkout(unsigned queue)
{
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator, *prev;
	for (estimator = estimator_list, prev = NULL; estimator; prev = estimator, estimator = estimator->next)
		if (estimator->queue == queue) break;
	if (estimator) {
		if (prev)
			prev->next = estimator->next;
		else
			estimator_list = estimator->next;
		llsp_dispose(estimator->llsp);
		free(estimator);
	}
	
	pthread_mutex_unlock(&estimator_lock);
	// TODO: notify scheduler
}

#pragma mark -


#pragma mark Job Management

void job_submit(unsigned target, double deadline, unsigned count, const double metrics[])
{
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->queue == target) break;
	if (!estimator)
		estimator = estimator_alloc(target);
	if (!estimator->llsp) {
		estimator->metrics_count = count;
		estimator->llsp = llsp_new(count);
	}
	
	assert(estimator->metrics_count == count);
	for (unsigned i = 0; i < estimator->metrics_count; i++) {
		*estimator->write_pos = metrics[i];
		if (++estimator->write_pos > estimator->metrics + estimator->size)
			estimator->write_pos = estimator->metrics;
		assert(estimator->write_pos != estimator->read_pos);  // FIXME: realloc when full
	}
	
	double prediction = 0.0;
	if (llsp_solve(estimator->llsp))
		prediction = llsp_predict(estimator->llsp, metrics);
	
	pthread_mutex_unlock(&estimator_lock);
	// TODO: notify scheduler
	
	static unsigned job_id = 0;
	printf("job %u submitted: %lf, %lf\n", job_id++, deadline, prediction);
}

void job_next(unsigned queue)
{
	double time;
#ifdef __linux__
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
#	warning falling back to gettimeofday() which includes blocking time, results may be wrong
	struct timeval tv;
    gettimeofday(&tv, NULL);
	time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
	
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->queue == queue) break;
	if (!estimator)
		estimator = estimator_alloc(queue);
	if (estimator->read_pos != estimator->write_pos) {
		double metrics[estimator->metrics_count];
		for (unsigned i = 0; i < estimator->metrics_count; i++) {
			metrics[i] = *estimator->read_pos;
			if (++estimator->read_pos > estimator->metrics + estimator->size)
				estimator->read_pos = estimator->metrics;
		}
		if (estimator->time > 0.0)
			llsp_add(estimator->llsp, metrics, time - estimator->time);
	}
	
	pthread_mutex_unlock(&estimator_lock);
	// TODO: notify scheduler
	
	static unsigned job_id = 0;
	printf("job %u finished: %lf, %lf\n", job_id++, time, time - estimator->time);
	estimator->time = time;
}

void job_start_blocking(unsigned queue)
{
#ifndef __linux__
	/* Linux does not need this, because it has per-thread CPU clocks
	 * that do not include blocking time */
	struct timeval tv;
    gettimeofday(&tv, NULL);
	double time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
	
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->queue == queue) break;
	if (estimator && estimator->time != 0.0)
		estimator->time -= time;
	
	pthread_mutex_unlock(&estimator_lock);
#endif
}

void job_stop_blocking(unsigned queue)
{
#ifndef __linux__
	/* Linux does not need this, because it has per-thread CPU clocks
	 * that do not include blocking time */
	struct timeval tv;
    gettimeofday(&tv, NULL);
	double time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
	
	pthread_mutex_lock(&estimator_lock);
	
	struct estimator_s *estimator;
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->queue == queue) break;
	if (estimator && estimator->time != 0.0)
		estimator->time += time;
	
	pthread_mutex_unlock(&estimator_lock);
#endif
}

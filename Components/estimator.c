/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#define BUFFER_TYPE double
#include "estimator.h"
#include "llsp.h"

struct estimator_s {
	struct estimator_s *next;
	pthread_mutex_t lock;
	void *code;
	
	double time;
	double previous_deadline;
	size_t metrics_count;
	llsp_t *llsp;
	
	struct buffer metrics;
};

static pthread_rwlock_t estimator_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct estimator_s *estimator_list = NULL;

static inline struct estimator_s *find_estimator(void *code)
{
	struct estimator_s *estimator;
	pthread_rwlock_rdlock(&estimator_lock);
	for (estimator = estimator_list; estimator; estimator = estimator->next)
		if (estimator->code == code) break;
	pthread_rwlock_unlock(&estimator_lock);
	return estimator;
}

#pragma mark -


#pragma mark Job Management

void atlas_job_submit(void *code, pid_t tid, atlas_job_t job)
{
	struct estimator_s *estimator = find_estimator(code);
	
	if (!estimator) {
		pthread_rwlock_wrlock(&estimator_lock);
		for (estimator = estimator_list; estimator; estimator = estimator->next)
			if (estimator->code == code) break;
		
		if (!estimator) {
			estimator = malloc(sizeof(struct estimator_s));
			if (!estimator) abort();
			pthread_mutex_init(&estimator->lock, NULL);
			
			estimator->code = code;
			estimator->time = 0.0;
			estimator->previous_deadline = 0.0;
			estimator->metrics_count = 0;
			estimator->llsp = NULL;
			
			buffer_init(&estimator->metrics);
			
			estimator->next = estimator_list;
			estimator_list = estimator;
		}
		pthread_rwlock_unlock(&estimator_lock);
	}
	
	pthread_mutex_lock(&estimator->lock);
	
	double offset = 0.0;
	if (job.reference == sched_deadline_relative)
		offset = atlas_now();
	else
		assert(job.reference == sched_deadline_absolute);
	
	if (job.deadline + offset < estimator->previous_deadline) {
//		fprintf(stderr, "WARNING: deadlines not ordered (%lf < %lf)\n", deadline, estimator->previous_deadline);
		job.deadline = estimator->previous_deadline - offset;
	}
	estimator->previous_deadline = job.deadline + offset;
	
	if (!estimator->llsp) {
		estimator->metrics_count = job.metrics_count;
		estimator->llsp = llsp_new(estimator->metrics_count + 1);  // add an extra 1-column
	}
	assert(estimator->metrics_count == job.metrics_count);
	
	double prediction = 0.0;
#if LLSP_PREDICT
	double llsp_metrics[estimator->metrics_count + 1];
	for (size_t i = 0; i < estimator->metrics_count + 1; i++) {
		if (i < estimator->metrics_count)
			llsp_metrics[i] = job.metrics[i];
		else
			llsp_metrics[i] = 1.0;  // add an extra 1-column
	}
	if (llsp_solve(estimator->llsp))
		prediction = llsp_predict(estimator->llsp, llsp_metrics);
#endif
	
	for (size_t i = 0; i < estimator->metrics_count; i++)
		buffer_put(&estimator->metrics, job.metrics[i]);
	buffer_put(&estimator->metrics, job.deadline + offset);  // always keep absolute deadlines
	buffer_put(&estimator->metrics, prediction);
	
#if JOB_SCHEDULING
	prediction *= JOB_OVERALLOCATION;
	struct timeval tv_deadline = {
		.tv_sec = job.deadline,
		.tv_usec = 1000000 * (job.deadline - (long long)job.deadline)
	};
	struct timeval tv_exectime = {
		.tv_sec = prediction,
		.tv_usec = 1000000 * (prediction - (long long)prediction)
	};
	sched_submit(tid, &tv_exectime, &tv_deadline, job.reference);
#endif
	
	pthread_mutex_unlock(&estimator->lock);
}

void atlas_job_next(void *code)
{
	double time = atlas_progress();
	struct estimator_s *estimator = find_estimator(code);
	assert(estimator);
	
	estimator->time = time;
	
#if JOB_SCHEDULING
	sched_next();
#endif
	if (hook_job_release)
		hook_job_release(code);
}

void atlas_job_train(void *code)
{
	double time = atlas_progress();
	struct estimator_s *estimator = find_estimator(code);
	assert(estimator);
	assert(estimator->time > 0.0);
	
	pthread_mutex_lock(&estimator->lock);
	
	if (estimator->metrics.read != estimator->metrics.write) {
		double llsp_metrics[estimator->metrics_count + 1];
		for (size_t i = 0; i < estimator->metrics_count + 1; i++) {
			if (i < estimator->metrics_count)
				llsp_metrics[i] = buffer_get(&estimator->metrics);
			else
				llsp_metrics[i] = 1.0;  // add an extra 1-column
		}
		double deadline = buffer_get(&estimator->metrics);  // absolute deadline
		double prediction = buffer_get(&estimator->metrics);
		double execution_time = time - estimator->time;
		
#if LLSP_PREDICT
		llsp_add(estimator->llsp, llsp_metrics, execution_time);
#endif
		if (hook_job_complete)
			hook_job_complete(code, time, deadline, prediction, execution_time);
	}
	
	pthread_mutex_unlock(&estimator->lock);
}

#pragma mark -


#pragma mark ATLAS Timebase

double atlas_now(void)
{
#ifdef __linux__
	/* ATLAS uses CLOCK_MONOTONIC as reference, because seconds are always a wallclock second there */
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

double atlas_progress(void)
{
#ifdef __linux__
	/* a per-thread clock, which only counts the time the thread is running */
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
#warning falling back to gettimeofday() which includes blocking and waiting time, results will be wrong
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

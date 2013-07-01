/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wpadded"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "dispatch.h"
#include "estimator.h"

struct detect_s {
	struct detect_s *next;
	double time;
	double completion;
};

struct job_s {
	struct job_s *next;       // job list ordered by deadline
	struct detect_s *detect;  // completion time predictions to detect deadline misses
	void *code;
	double deadline;
	double prediction;
	bool running;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct job_s *job_list = NULL;


/* predicts completion times to detect deadline misses,
 * iterates over all preceding jobs and aggregates their execution times,
 * call with the job list lock held */
static void update_job_list(void)
{
	double now = atlas_now();
	
	for (struct job_s *current = job_list; current; current = current->next) {
		// abort updating if any job has no execution time prediction
		// or a longish job is currently running (we don't know how long it already executed)
		if (current->prediction == 0.0 || (current->running && current->prediction > 0.001))
			return;
		
		double cumulative = 0.0;
		for (struct job_s *job = job_list; job != current->next; job = job->next)
			cumulative += job->prediction;
		
		struct detect_s *node = malloc(sizeof(struct detect_s));
		if (!node) abort();
		node->time = now;
		node->completion = now + cumulative;
		node->next = current->detect;
		current->detect = node;
	}
}


void hook_job_submit(void *code, double prediction, double reservation, double deadline)
{
	(void)reservation;
	
	/* create new job node */
	struct job_s *submit = malloc(sizeof(struct job_s));
	if (!submit) abort();
	submit->detect = NULL;
	submit->code = code;
	submit->deadline = deadline;
	submit->prediction = prediction;
	submit->running = false;
	
	pthread_mutex_lock(&lock);
	
	/* insert node in deadline position */
	struct job_s *job;
	for (job = job_list; job; job = job->next)
		if (!job->next || job->next->deadline > deadline) break;
	if (job) {
		submit->next = job->next;
		job->next = submit;
	} else {
		submit->next = NULL;
		job_list = submit;
	}
	
	update_job_list();
	
	pthread_mutex_unlock(&lock);
}

void hook_job_release(void *code)
{
	(void)code;
	
	pthread_mutex_lock(&lock);
	
	update_job_list();
	
	/* mark released job as running */
	struct job_s *job;
	for (job = job_list; job; job = job->next)
		if (job->code == code) break;
	assert(job);
	job->running = true;
	
	pthread_mutex_unlock(&lock);	
}

void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	(void)thread_time;
	(void)execution_time;
	
	pthread_mutex_lock(&lock);
	
	/* find completed job */
	struct job_s *job, *prev = NULL;
	for (job = job_list; job; prev = job, job = job->next)
		if (job->code == code &&
		    job->deadline == deadline &&
		    job->prediction == prediction)
			break;
	assert(job);
	
	/* evaluate completion time prediction */
	double now = atlas_now();
	dispatch_queue_t queue = dispatch_get_current_queue();
	const char *label = dispatch_queue_get_label(queue);
	if (!queue || strcmp(label, "video") == 0) {
		for (struct detect_s *node = job->detect; node; node = node->next) {
			if (node->completion > job->deadline || now > job->deadline)
				printf("%d %lf %lf\n",
					// -1: false negative, +1: false positive, 0: correctly predicted
					(node->completion > job->deadline) - (now > job->deadline),
					node->time - now, node->completion - now);
		}
	}
	
	/* delete completed job */
	for (struct detect_s *node = job->detect; node;) {
		struct detect_s *tmp = node->next;
		free(node);
		node = tmp;
	}
	if (prev)
		prev->next = job->next;
	else
		job_list = job->next;
	free(job);
	
	update_job_list();
	
	pthread_mutex_unlock(&lock);
}

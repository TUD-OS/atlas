/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>

#include "dispatch.h"
#include "estimator.h"

static const char * const queues[4] = { "read", "video", "refresh", NULL };
static double release_time[4] = { 0.0 };


void hook_job_release(void *code)
{
	(void)code;
	release_time[dispatch_queue_id(queues)] = atlas_now();
}

void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	(void)code;
	(void)prediction;
	
	size_t id = dispatch_queue_id(queues);
	double time = atlas_now();
	printf("%zd, %lf, %lf, %lf, %lf\n",
		   id,
		   time, thread_time,  // for calculating CPU share
		   execution_time,  // for execution time variability
		   deadline - release_time[id] - execution_time);  // slack
}

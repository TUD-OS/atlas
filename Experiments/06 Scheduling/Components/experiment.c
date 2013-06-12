/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>
#include <string.h>

#include "dispatch.h"
#include "estimator.h"


void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	(void)code;
	(void)thread_time;
	(void)deadline;
	(void)prediction;
	(void)execution_time;
	
	static double start_time = 0.0;
	static double previous_completion = 0.0;
	
	dispatch_queue_t queue = dispatch_get_current_queue();
	const char *label = dispatch_queue_get_label(queue);
	
	if (label && strcmp(label, "refresh") == 0) {
		double time = atlas_now();
		if (start_time == 0.0)
			start_time = time;
		if (previous_completion > 0.0)
			printf("%lf %lf\n", time - start_time, time - previous_completion);
		previous_completion = time;
	}
}

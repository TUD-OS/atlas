/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
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
	
	dispatch_queue_t queue = dispatch_get_current_queue();
	const char *label = dispatch_queue_get_label(queue);
	
	if (label && strcmp(label, "video") == 0)
		/* decoder stage */
		printf("%lf %lf\n", prediction, execution_time);
}

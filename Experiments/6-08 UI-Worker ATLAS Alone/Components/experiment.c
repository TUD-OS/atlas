/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>
#include "estimator.h"

void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	(void)code;
	(void)thread_time;
	(void)prediction;
	(void)execution_time;
	
	printf("%lf\n", atlas_now() - deadline + 0.1);
}

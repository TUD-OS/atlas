/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>
#include "jobs.h"


void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	if (code == ffplay_stages[1].code)
		/* decoder stage */
		printf("%lf %lf\n", prediction, execution_time);
}


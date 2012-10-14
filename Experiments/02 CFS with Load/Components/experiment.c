/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>

#include "libavformat/avformat.h"
#include "jobs.h"


void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	if (code == ffplay_stages[2].code) {
		/* refresh stage */
		static double previous_completion = 0.0;
		double time = av_gettime() / 1000000.0;
		if (previous_completion > 0.0)
			printf("%lf, %lf\n", time, time - previous_completion);
		previous_completion = time;
	}
}

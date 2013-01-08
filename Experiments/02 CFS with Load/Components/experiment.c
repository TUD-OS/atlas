/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>

#include "libavformat/avformat.h"
#include "jobs.h"


void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	static double start_time = 0.0;
	static double previous_completion = 0.0;
	
	if (code == ffplay_stages[2].code) {
		/* refresh stage */
		double time = av_gettime() / 1000000.0;
		if (start_time == 0.0)
			start_time = time;
		if (previous_completion > 0.0)
			printf("%lf %lf\n", time - start_time, time - previous_completion);
		previous_completion = time;
	}
}

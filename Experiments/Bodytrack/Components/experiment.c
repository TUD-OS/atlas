/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "estimator.h"

static double start_time;
static bool log_times = false;
static bool fail = false;

static void start(void) __attribute__((constructor));
static void start(void)
{
	if (getenv("FAIL")) fail = true;
	if (getenv("LOG_TIMES")) log_times = true;
	start_time = atlas_now();
	if (log_times)
		puts("0.000000 0 0.000000 0.000000 0.000000");
	else
		puts("0.000000 0");
}

void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution_time)
{
	(void)code;
	(void)thread_time;
	(void)prediction;
	(void)execution_time;
	
	static size_t progress = 0;
	printf("%lf %zd", atlas_now() - start_time, ++progress);
	
	if (log_times)
		printf(" %lf %lf %lf\n", atlas_now() - deadline + 0.25, prediction, execution_time);
	else
		printf("\n");
	
	if (fail && progress == 120) {
		/* simulate failing into an endless loop */
		while (atlas_now() < start_time + 100);
		printf("%lf %zd\n",  atlas_now() - start_time, progress);
		exit(0);
	}
	
	/* simulate periodic task: wait until the next deadline */
	int sleep_result = 0;
	do {
		double wait = deadline - atlas_now();
		if (wait > 0)
			sleep_result = usleep((useconds_t)(wait * 1000000));
	} while (sleep_result < 0);
}

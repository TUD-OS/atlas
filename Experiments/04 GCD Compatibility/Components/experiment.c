/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#ifdef __linux__
#  include <time.h>
#else
#  include <sys/time.h>
#endif

static void stop(void) __attribute__((destructor));

static void stop(void)
{
	double time;
	
#ifdef __linux__
	/* a per-process clock, which only counts the time spent by this application */
	struct timespec ts;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
	time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
#pragma message "falling back to gettimeofday() which includes blocking and waiting time, results will be wrong"
	struct timeval tv;
	gettimeofday(&tv, NULL);
	time = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
	
	printf("%lf\n", time);
}

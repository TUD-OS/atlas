/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#ifdef __linux__
#	include <time.h>
#else
#	include <sys/time.h>
#endif

#include "process.h"


static void measure_times(AVCodecContext *c);

static inline double get_time(void)
{
#ifdef __linux__
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}


void process_init(AVCodecContext *c, const char *file)
{
	(void)file;
	
	c->thread_count = 1;
	c->process_slice = (void (*)(void *))measure_times;
}

void process_finish(AVCodecContext *c)
{
	(void)c;
}

static void measure_times(AVCodecContext *c)
{
	static double time;
	
	if (c->metrics.type == PSEUDO_SLICE_FRAME_START)
		time = get_time();
	if (c->metrics.type == PSEUDO_SLICE_FRAME_END)
		printf("%lf\n", get_time() - time);
}

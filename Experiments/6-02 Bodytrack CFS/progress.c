/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "estimator.h"

static double start_time;
static double progress;

static void handler(int signal)
{
	switch (signal) {
	case SIGALRM:
		printf("%lf %lf\n", atlas_now() - start_time, atlas_progress() - progress);
		progress = atlas_progress();
		break;
	case SIGTERM:
		exit(0);
	}
}


int main(void)
{
	struct sigaction action = {
		.sa_handler = handler
	};
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGALRM, &action, NULL);
	
	struct itimerval timer = {
		.it_interval.tv_sec  = 1,
		.it_interval.tv_usec = 0,
		.it_value.tv_sec     = 1,
		.it_value.tv_usec    = 0
	};
	setitimer(ITIMER_REAL, &timer, NULL);
	
	start_time = atlas_now();
	progress = atlas_progress();
	while (1);
}

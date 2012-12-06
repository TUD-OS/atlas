/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include "estimator.h"

static void start(void) __attribute__((constructor));
static void stop(void) __attribute__((destructor));

static double start_time;


static void start(void)
{
	start_time = atlas_now();
}

static void stop(void)
{
	printf("%lf\n", atlas_now() - start_time);
}


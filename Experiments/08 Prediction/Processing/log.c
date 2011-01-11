/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include "process.h"

static double measured_decoding_time;


void hook_slice_any(const AVCodecContext *c)
{
	measured_decoding_time += get_time();
	if (!proc.frame) return;
	if (c->metrics.type >= 0)
		printf("%f, %lf\n",
			   proc.frame->slice[proc.frame->slice_count].decoding_time,
			   measured_decoding_time);
}

void hook_slice_end(const AVCodecContext *c)
{
	/* setup timer for next slice */
	measured_decoding_time = -get_time();
}

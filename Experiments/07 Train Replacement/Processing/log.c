/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include "process.h"


void hook_frame_end(const AVCodecContext *c)
{
	int slice;
	
#if 0
	for (slice = 0; slice < proc.frame->slice_count; slice++) {
		printf("D: %lf, %lf\n");
	}
#endif
}

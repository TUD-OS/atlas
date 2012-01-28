/*
 * Copyright (C) 2006 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>

#include "process.h"

FILE *result;

static void process_slice(AVCodecContext *c);


void process_init(AVCodecContext *c, const char *file)
{
	result = fopen("Visualization.yuv", "w");
	c->debug_mv |= FF_DEBUG_VIS_MV_P_FOR | FF_DEBUG_VIS_MV_B_FOR | FF_DEBUG_VIS_MV_B_BACK;
#if 0
	/* enable to visualize macroblock types as well */
	c->debug    |= FF_DEBUG_VIS_MB_TYPE;
#endif
	c->process_slice = (void (*)(void *))process_slice;
	c->process_sideband = NULL;
}

void process_finish(AVCodecContext *c)
{
}

static void process_slice(AVCodecContext *c)
{
	int y;
	if (c->metrics.type != -2 || !c->frame.display) return;
	for (y = 0; y < c->height; y++)
		fwrite(c->frame.display->data[0] + y * c->frame.display->linesize[0], 1, c->width, result);
	for (y = 0; y < c->height / 2; y++)
		fwrite(c->frame.display->data[1] + y * c->frame.display->linesize[1], 1, c->width / 2, result);
	for (y = 0; y < c->height / 2; y++)
		fwrite(c->frame.display->data[2] + y * c->frame.display->linesize[2], 1, c->width / 2, result);
}

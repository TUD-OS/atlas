/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"


#ifdef __APPLE__

/* The Darwin linker wants to have a default implementation for all weak symbols,
 * otherwise, strange things happen (read: crashes). This is not needed on Linux. */
int frame_storage_alloc(AVCodecContext *c, AVFrame *frame)
{
	return avcodec_default_get_buffer(c, frame);
}

void frame_storage_destroy(AVCodecContext *c, AVFrame *frame)
{
	avcodec_default_release_buffer(c, frame);
}

void hook_slice_any(const AVCodecContext *c)
{
}

void hook_slice_end(const AVCodecContext *c)
{
}

void hook_frame_end(const AVCodecContext *c)
{
}

#endif

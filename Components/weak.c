/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

/* The Darwin linker wants to have a default implementation for all weak symbols. This is not needed on Linux. */
#ifdef __APPLE__

#pragma clang diagnostic ignored "-Wgcc-compat"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include "libavcodec/avcodec.h"

void hook_job_submit(void *code, double prediction, double reservation, double deadline) __attribute__((weak)) {}
void hook_job_release(void *code) __attribute__((weak)) {}
void hook_job_complete(void *code, double thread_time, double deadline, double prediction, double execution) __attribute__((weak)) {}
void hook_llsp_result(const double *result, size_t count) __attribute__((weak)) {}
void hook_slice_any(const AVCodecContext *c) __attribute__((weak)) {}
void hook_slice_end(const AVCodecContext *c) __attribute__((weak)) {}
void hook_frame_end(const AVCodecContext *c) __attribute__((weak)) {}

int frame_storage_alloc(AVCodecContext *c, AVFrame *frame) __attribute__((weak))
{
	return avcodec_default_get_buffer(c, frame);
}

void frame_storage_destroy(AVCodecContext *c, AVFrame *frame) __attribute__((weak))
{
	avcodec_default_release_buffer(c, frame);
}

#endif

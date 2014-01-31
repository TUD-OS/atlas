/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <dispatch/dispatch.h>
#include <libavutil/timer.h>
#include "process.h"

#if !FFMPEG_TIME || !FFMPEG_METRICS
#error  FFmpeg is not configured correctly, check avcodec.h
#endif


void hook_slice_any(const AVCodecContext *c)
{
	static dispatch_once_t predicate;
	static float usecs_per_tsc = 0.0;
	
	dispatch_once(&predicate, ^{
		struct timeval tv;
		
		gettimeofday(&tv, NULL);
		double usecs_start = 1000000.0 * tv.tv_sec + (double)tv.tv_usec;
		uint_fast64_t tsc_start = read_time();
		usleep(1000000);
		uint_fast64_t tsc_stop = read_time();
		gettimeofday(&tv, NULL);
		double usecs_stop = 1000000.0 * tv.tv_sec + (double)tv.tv_usec;
		
		usecs_per_tsc = (float)((usecs_stop - usecs_start) / (tsc_stop - tsc_start));
	});
	
	printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ",
	       c->metrics.type,                              //  1
	       (c->slice.end_index - c->slice.start_index),  //  2
	       c->metrics.bits_cabac,                        //  3
	       c->metrics.bits_cavlc,                        //  4
	       c->metrics.intra_4x4,                         //  5
	       c->metrics.intra_8x8,                         //  6
	       c->metrics.intra_16x16,                       //  7
	       c->metrics.inter_4x4,                         //  8
	       c->metrics.inter_8x8,                         //  9
	       c->metrics.inter_16x16,                       // 10
	       c->metrics.idct_pcm,                          // 11
	       c->metrics.idct_4x4,                          // 12
	       c->metrics.idct_8x8,                          // 13
	       c->metrics.deblock_edges);                    // 14
	printf("%f, %f, %f, %f, %f, %f\n",
	       usecs_per_tsc * c->timing.decompression,      // 15
	       usecs_per_tsc * c->timing.spatial,            // 16
	       usecs_per_tsc * c->timing.temporal,           // 17
	       usecs_per_tsc * c->timing.transform,          // 18
	       usecs_per_tsc * c->timing.post,               // 19
	       usecs_per_tsc * c->timing.total);             // 20
}

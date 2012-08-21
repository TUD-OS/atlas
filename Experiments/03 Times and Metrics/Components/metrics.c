/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include "process.h"

#if !FFMPEG_TIME || !FFMPEG_METRICS
#error  FFmpeg is not configured correctly, check avcodec.h
#endif


void hook_slice_any(const AVCodecContext *c)
{
	printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ",
		   c->metrics.type,                              //  1
		   (c->slice.end_index - c->slice.start_index),  //  2
		   c->metrics.bits,                              //  3
		   c->metrics.intra_pcm,                         //  4
		   c->metrics.intra_4x4,                         //  5
		   c->metrics.intra_8x8,                         //  6
		   c->metrics.intra_16x16,                       //  7
		   c->metrics.inter_4x4,                         //  8
		   c->metrics.inter_8x8,                         //  9
		   c->metrics.inter_16x16,                       // 10
		   c->metrics.idct_4x4,                          // 11
		   c->metrics.idct_8x8,                          // 12
		   c->metrics.deblock_edges);                    // 13
	printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", ",
		   c->timing.decoder_prep,                       // 14
		   c->timing.decompress_cabac,                   // 15
		   c->timing.decompress_cavlc,                   // 16
		   c->timing.spatial_pred,                       // 17
		   c->timing.temporal_pred,                      // 18
		   c->timing.idct,                               // 19
		   c->timing.post);                              // 20
	printf("%" PRIu64 "\n", c->timing.total);            // 21
}

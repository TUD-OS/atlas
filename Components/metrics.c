/*
 * Copyright (C) 2006-2012 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "process.h"


#if METRICS_EXTRACT
void remember_metrics(const AVCodecContext *c)
{
	proc.frame->slice[proc.frame->slice_count].metrics.type          = c->metrics.type;
	proc.frame->slice[proc.frame->slice_count].metrics.bits          = c->metrics.bits;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_pcm     = c->metrics.intra_pcm;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_4x4     = c->metrics.intra_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_8x8     = c->metrics.intra_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_16x16   = c->metrics.intra_16x16;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_4x4     = c->metrics.inter_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_8x8     = c->metrics.inter_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_16x16   = c->metrics.inter_16x16;
	proc.frame->slice[proc.frame->slice_count].metrics.idct_4x4      = c->metrics.idct_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.idct_8x8      = c->metrics.idct_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.deblock_edges = c->metrics.deblock_edges;
}
#endif

#if METADATA_WRITE && (METRICS_EXTRACT || METADATA_READ)
void write_metrics(const frame_node_t *frame, int slice)
{
	nalu_write_uint8(proc.metadata.write, frame->slice[slice].metrics.type);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.bits);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.intra_pcm);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.intra_4x4);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.intra_8x8);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.intra_16x16);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.inter_4x4);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.inter_8x8);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.inter_16x16);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.idct_4x4);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.idct_8x8);
	nalu_write_uint24(proc.metadata.write, frame->slice[slice].metrics.deblock_edges);
}
#endif

#if METADATA_READ
void read_metrics(frame_node_t *frame, int slice)
{
	frame->slice[slice].metrics.type          = nalu_read_uint8(proc.metadata.read);
	frame->slice[slice].metrics.bits          = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.intra_pcm     = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.intra_4x4     = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.intra_8x8     = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.intra_16x16   = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.inter_4x4     = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.inter_8x8     = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.inter_16x16   = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.idct_4x4      = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.idct_8x8      = nalu_read_uint24(proc.metadata.read);
	frame->slice[slice].metrics.deblock_edges = nalu_read_uint24(proc.metadata.read);
}
#endif

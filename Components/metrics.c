/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <assert.h>
#include "process.h"


#if METRICS_EXTRACT
void remember_metrics(const AVCodecContext *c)
{
	assert(c->metrics.type >= 0);
	proc.frame->slice[proc.frame->slice_count].metrics.type          = (size32_t)c->metrics.type;
	proc.frame->slice[proc.frame->slice_count].metrics.bits_cabac    = c->metrics.bits_cabac;
	proc.frame->slice[proc.frame->slice_count].metrics.bits_cavlc    = c->metrics.bits_cavlc;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_4x4     = c->metrics.intra_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_8x8     = c->metrics.intra_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.intra_16x16   = c->metrics.intra_16x16;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_4x4     = c->metrics.inter_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_8x8     = c->metrics.inter_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.inter_16x16   = c->metrics.inter_16x16;
	proc.frame->slice[proc.frame->slice_count].metrics.idct_pcm      = c->metrics.idct_pcm;
	proc.frame->slice[proc.frame->slice_count].metrics.idct_4x4      = c->metrics.idct_4x4;
	proc.frame->slice[proc.frame->slice_count].metrics.idct_8x8      = c->metrics.idct_8x8;
	proc.frame->slice[proc.frame->slice_count].metrics.deblock_edges = c->metrics.deblock_edges;
}
#endif

#if METADATA_WRITE && (METRICS_EXTRACT || METADATA_READ)
void write_metrics(const frame_node_t *frame, size32_t slice)
{
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.type);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.bits_cabac);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.bits_cavlc);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.intra_4x4);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.intra_8x8);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.intra_16x16);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.inter_4x4);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.inter_8x8);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.inter_16x16);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.idct_pcm);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.idct_4x4);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.idct_8x8);
	nalu_write_unsigned(proc.metadata.write, frame->slice[slice].metrics.deblock_edges);
}
#endif

#if METADATA_READ
void read_metrics(frame_node_t *frame, size32_t slice)
{
	frame->slice[slice].metrics.type          = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.bits_cabac    = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.bits_cavlc    = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.intra_4x4     = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.intra_8x8     = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.intra_16x16   = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.inter_4x4     = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.inter_8x8     = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.inter_16x16   = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.idct_pcm      = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.idct_4x4      = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.idct_8x8      = nalu_read_unsigned(proc.metadata.read);
	frame->slice[slice].metrics.deblock_edges = nalu_read_unsigned(proc.metadata.read);
}
#endif

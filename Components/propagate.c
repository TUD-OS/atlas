/*
 * Copyright (C) 2006-2015 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <assert.h>
#include "process.h"

#ifdef SCHEDULE_EXECUTE
static void update_total_error(const AVCodecContext *c, const AVPicture *quad);
#endif


#if PREPROCESS
#include "libavcodec/mpegvideo.h"

/* upcasting an AVFrame's opaque pointer */
static inline frame_node_t *private_data(const AVFrame *frame)
{
	return *(frame_node_t **)frame->opaque;
}

void remember_dependencies(const AVCodecContext *c)
{
	int mb, list, i, x, y, ref, slice;
	
	if (proc.frame->slice_count == 0) {
		proc.frame->reference_lifetime = 0;
		/* first copy the current reference stacks */
		for (ref = -REF_MAX; ref <= REF_MAX; ref++) {
			if (ref > 0 && ref - 1 < c->reference.short_count)
				proc.frame->reference[ref] = private_data(c->reference.short_list[ref - 1]);
			else if (ref < 0 && -ref - 1 < c->reference.long_count)
				proc.frame->reference[ref] = private_data(c->reference.long_list[-ref - 1]);
			else
				proc.frame->reference[ref] = NULL;
			/* increment the lifetime of the references we can still see */
			if (proc.frame->reference[ref]) proc.frame->reference[ref]->reference_lifetime++;
		}
	}
	
	/* spatial prediction dependencies are ignored, because I have not observed them */
	
	for (mb = proc.frame->slice[proc.frame->slice_count].start_index;
		 mb < proc.frame->slice[proc.frame->slice_count].end_index; mb++) {
		const int mb_y = mb / proc.mb_width;
		const int mb_x = mb % proc.mb_width;
		const int mb_stride = proc.mb_width + 1;
		const int mb_index = mb_x + mb_y * mb_stride;
		const int ref_stride = 2 * proc.mb_width;
		const int ref_index_base = 2*mb_x + 2*mb_y * ref_stride;
		const int mv_sample_log2 = 4 - c->frame.current->motion_subsample_log2;
		const int mv_stride = proc.mb_width << mv_sample_log2;
		const int mv_index_base = (mb_x << mv_sample_log2) + (mb_y << mv_sample_log2) * mv_stride;
		const int mb_type = c->frame.current->mb_type[mb_index];
		for (list = 0; list < 2; list++) {
			for (i = 0; i < 16; i++) {
				const int ref_index = ref_index_base + ((i>>1)&1) + (i>>3) * ref_stride;
				const int mv_index = mv_index_base + (i&3) + (i>>2) * mv_stride;
				if (proc.ref_num[list][ref_index]) {
					const frame_node_t *frame = proc.frame->reference[proc.ref_num[list][ref_index]];
					if ((IS_16X16(mb_type) && (i&15) == 0) ||
						(IS_16X8(mb_type) && (i&(15-8)) == 0) ||
						(IS_8X16(mb_type) && (i&(15-2)) == 0) ||
						(IS_8X8(mb_type) && (i&(15-8-2)) == 0)) {
						/* FIXME: we cannot distinguish the 8x8, 8x4, 4x8 and 4x4 subtypes */
						const int start_x = (mb_x << mb_size_log) + c->frame.current->motion_val[list][mv_index][0] / 4;
						const int start_y = (mb_y << mb_size_log) + c->frame.current->motion_val[list][mv_index][1] / 4;
						const int size_x = (IS_16X16(mb_type) || IS_16X8(mb_type)) ? 16 : 8;
						const int size_y = (IS_16X16(mb_type) || IS_8X16(mb_type)) ? 16 : 8;
						/* check every individual pixel of this motion block */
						for (y = start_y; y < start_y + size_y; y++) {
							for (x = start_x; x < start_x + size_x; x++) {
								int clip_x = x >> mb_size_log;
								int clip_y = y >> mb_size_log;
								if (clip_x < 0) x = 0;
								if (clip_y < 0) y = 0;
								if (clip_x >= proc.mb_width) clip_x = proc.mb_width - 1;
								if (clip_y >= proc.mb_height) clip_y = proc.mb_height - 1;
								const int mb_index = clip_x + clip_y * proc.mb_width;
								/* search for the slice this pixel came from */
								for (slice = 0; slice < frame->slice_count; slice++) {
									if (mb_index >= frame->slice[slice].start_index && mb_index < frame->slice[slice].end_index) {
										/* add .5 for bi-prediction, 1 for normal prediction */
										float contrib = .5 + .5 * (float)!proc.ref_num[list ^ 1][ref_index];
										proc.frame->slice[proc.frame->slice_count].immission[proc.ref_num[list][ref_index]][slice] += contrib;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	for (ref = -REF_MAX; ref <= REF_MAX; ref++) {
		const frame_node_t *reference = proc.frame->reference[ref];
		if (!reference) {
			for (slice = 0; slice < SLICE_MAX; slice++)
				assert(proc.frame->slice[proc.frame->slice_count].immission[ref][slice] == 0.0);
			continue;
		}
		for (slice = 0; slice < SLICE_MAX; slice++) {
			/* scale the propagation factors by the size of the reference slice,
			 * so that copying an entire reference-slice completely will result in factor 1 for that slice */
			const int slice_mbs = reference->slice[slice].end_index - reference->slice[slice].start_index;
			const int slice_pixels = (1 << (2 * mb_size_log)) * slice_mbs;
			proc.frame->slice[proc.frame->slice_count].immission[ref][slice] /= slice_pixels;
		}
	}
}

void accumulate_quality_loss(frame_node_t *frame)
{
	frame_node_t *future;
	int slice_here, slice_there, ref;
	
	if (!proc.frame || frame == proc.frame->next) return;
	
	/* accumulate backwards, so do recursion first */
	accumulate_quality_loss(frame->next);
	
	/* initialize with ones */
	for (slice_here = 0; slice_here < frame->slice_count; slice_here++)
		frame->slice[slice_here].emission_factor = 1.0;
	
	/* search all future frames for direct references to this one */
	for (future = frame->next; future; future = future->next) {
		for (ref = -REF_MAX; ref <= REF_MAX; ref++) {
			if (future->reference[ref] == frame) {
				/* this future frame references our frame with reference number ref */
				for (slice_there = 0; slice_there < future->slice_count; slice_there++) {
					for (slice_here = 0; slice_here < frame->slice_count; slice_here++) {
						/* this is the factor, by which errors in slice_here are propagated into slice_there directly */
						const float propagation_level1 = future->slice[slice_there].immission[ref][slice_here];
						/* this is the factor, by which errors in slice_there are propagated further ahead,
						 * since the recursion has already worked on this frame, we can use this value here */
						const float propagation_level2 = future->slice[slice_there].emission_factor;
						/* the complete quality loss factor along this propagation path */
						const float propagation_path = propagation_level1 * propagation_level2;
						/* accumulate */
						frame->slice[slice_here].emission_factor += propagation_path;
					}
				}
			}
		}
	}
}
#endif

#if PREPROCESS
int frame_storage_alloc(AVCodecContext *c, AVFrame *frame)
{
	frame->opaque = av_malloc(sizeof(frame_node_t *));
	*(void **)frame->opaque = NULL;
	return avcodec_default_get_buffer(c, frame);
}
void frame_storage_destroy(AVCodecContext *c, AVFrame *frame)
{
	frame_node_t *node = private_data(frame);
	av_freep(&frame->opaque);
	if (node && !--node->reference_count)
		av_free(node);
	avcodec_default_release_buffer(c, frame);
}
#endif

#ifdef SCHEDULE_EXECUTE
void propagation_visualize(const AVCodecContext *c)
{
	int y;
	static FILE *original = NULL;
	if (!original) original = fopen("Original.yuv", "r");
	
	if (!c->frame.display) return;
	
	AVPicture quad = proc.propagation.vis_frame;
	static FILE *vis = NULL;
	
	/* left upper quadrant: original */
	/* Y */
	for (y = 0; y < c->height; y++)
		fread(quad.data[0] + y * quad.linesize[0], sizeof(uint8_t), c->width, original);
	/* Cb */
	for (y = 0; y < c->height / 2; y++)
		fread(quad.data[1] + y * quad.linesize[1], sizeof(uint8_t), c->width / 2, original);
	/* Cr */
	for (y = 0; y < c->height / 2; y++)
		fread(quad.data[2] + y * quad.linesize[2], sizeof(uint8_t), c->width / 2, original);
	/* right upper quadrant: with propagated error */
	quad.data[0] += c->width;
	quad.data[1] += c->width / 2;
	quad.data[2] += c->width / 2;
	if (c->frame.display->coded_picture_number < (unsigned)proc.schedule.first_to_drop)
		av_picture_copy(&quad, (const AVPicture *)c->frame.display, PIX_FMT_YUV420P, c->width, c->height);
	update_total_error(c, &quad);
	
	if (!vis) vis = fopen("Visualization.yuv", "w");
	/* Y */
	for (y = 0; y < c->height; y++)
		fwrite(quad.data[0] + y * quad.linesize[0], sizeof(uint8_t), c->width, vis);
	/* Cb */
	for (y = 0; y < c->height / 2; y++)
		fwrite(quad.data[1] + y * quad.linesize[1], sizeof(uint8_t), c->width / 2, vis);
	/* Cr */
	for (y = 0; y < c->height / 2; y++)
		fwrite(quad.data[2] + y * quad.linesize[2], sizeof(uint8_t), c->width / 2, vis);
}
static void update_total_error(const AVCodecContext *c, const AVPicture *quad)
{
	picture_t original, degraded;
	
	/* the original frame is in the upper left quadrant */
	original.Y  = proc.propagation.vis_frame.data[0];
	original.Cb = proc.propagation.vis_frame.data[1];
	original.Cr = proc.propagation.vis_frame.data[2];
	original.line_stride_Y  = proc.propagation.vis_frame.linesize[0];
	original.line_stride_Cb = proc.propagation.vis_frame.linesize[1];
	original.line_stride_Cr = proc.propagation.vis_frame.linesize[2];
	original.width  = c->width;
	original.height = c->height;
	
	/* the degraded version is in the current quadrant */
	degraded.Y  = quad->data[0];
	degraded.Cb = quad->data[1];
	degraded.Cr = quad->data[2];
	degraded.line_stride_Y  = quad->linesize[0];
	degraded.line_stride_Cb = quad->linesize[1];
	degraded.line_stride_Cr = quad->linesize[2];
	degraded.width  = c->width;
	degraded.height = c->height;
	
	proc.propagation.total_error += ssim_quality_loss(&original, &degraded, NULL, ssim_precision);
}
#endif

#if METADATA_WRITE && PREPROCESS && !METADATA_READ
void write_immission(const frame_node_t *frame)
{
	uint8_t buf[sizeof(uint32_t)];
	int slice_here, slice_there, ref;
	
	if (!proc.metadata.propagation) return;
	
	for (slice_here = 0; slice_here < frame->slice_count; slice_here++) {
		for (ref = -REF_MAX; ref <= REF_MAX; ref++) {
			for (slice_there = 0; slice_there < SLICE_MAX; slice_there++) {
				if (frame->slice[slice_here].immission[ref][slice_there] > 0.0) {
					union {
						float in;
						uint32_t out;
					} convert;
					
					assert(sizeof(float) == sizeof(uint32_t));
					convert.in = frame->slice[slice_here].immission[ref][slice_there];
					buf[0] = (convert.out >> 24) & 0xFF;
					buf[1] = (convert.out >> 16) & 0xFF;
					buf[2] = (convert.out >>  8) & 0xFF;
					buf[3] = (convert.out >>  0) & 0xFF;
					fwrite(buf, sizeof(buf[0]), 4, proc.metadata.propagation);
					buf[0] = (uint8_t)ref;
					buf[1] = slice_there;
					fwrite(buf, sizeof(buf[0]), 2, proc.metadata.propagation);
				}
			}
		}
		/* end marker */
		buf[0] = buf[1] = buf[2] = buf[3] = 0;
		fwrite(buf, sizeof(buf[0]), 4, proc.metadata.propagation);
	}
	buf[0] = (frame->reference_lifetime >> 24) & 0xFF;
	buf[1] = (frame->reference_lifetime >> 16) & 0xFF;
	buf[2] = (frame->reference_lifetime >>  8) & 0xFF;
	buf[3] = (frame->reference_lifetime >>  0) & 0xFF;
	fwrite(buf, sizeof(buf[0]), 4, proc.metadata.propagation);
}
#endif

#if METADATA_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME)
void read_immission(frame_node_t *frame)
{
	uint8_t buf[sizeof(uint32_t)];
	int slice_here, slice_there, ref;
	
	/* clear and init the dependency storage */
	for (slice_here = 0; slice_here < SLICE_MAX + 1; slice_here++) {
		memset(frame->slice[slice_here].immission_base, 0, sizeof(frame->slice[0].immission_base));
		frame->slice[slice_here].immission = frame->slice[slice_here].immission_base + REF_MAX;
	}
	
	if (!proc.metadata.propagation) return;
	
	for (slice_here = 0; slice_here < frame->slice_count; slice_here++) {
		while (1) {
			union {
				uint32_t in;
				float out;
			} convert;
			
			assert(sizeof(float) == sizeof(uint32_t));
			fread(buf, sizeof(buf[0]), 4, proc.metadata.propagation);
			convert.in = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 0);
			if (!convert.in) break;
			fread(buf, sizeof(buf[0]), 2, proc.metadata.propagation);
			ref = (int8_t)buf[0];
			slice_there = buf[1];
			frame->slice[slice_here].immission[ref][slice_there] = convert.out;
		}
	}
	fread(buf, sizeof(buf[0]), 4, proc.metadata.propagation);
	frame->reference_lifetime = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 0);
}
#endif

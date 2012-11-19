/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#define IMPLEMENTS_HOOKS

#include "process.h"
#include "digits.h"


#pragma mark Frame Storage Management

int frame_storage_alloc(AVCodecContext *c, AVFrame *frame)
{
	/* round width and height to integer macroblock multiples */
	const int width  = (2 * (c->width  + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	const int height = (2 * (c->height + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	frame->opaque = av_malloc(avpicture_get_size(c->pix_fmt, width, height));
	return avcodec_default_get_buffer(c, frame);
}

void frame_storage_destroy(AVCodecContext *c, AVFrame *frame)
{
	av_freep(&frame->opaque);
	avcodec_default_release_buffer(c, frame);
}

/* upcasting an AVFrame's opaque pointer */
static inline uint8_t *private_data(const AVFrame *frame)
{
	return (uint8_t *)frame->opaque;
}

#pragma mark -


#pragma mark Helpers

static void draw_slice_error(AVPicture *frame)
{
	int slice, mb, y, x;
	
	for (slice = 0; slice < proc.frame->slice_count; slice++) {
		for (mb = proc.frame->slice[slice].start_index; mb < proc.frame->slice[slice].end_index; mb++) {
			const int mb_x = mb % proc.mb_width;
			const int mb_y = mb / proc.mb_width;
			const byte_block_t error = byte_spread *
			((5 * proc.frame->slice_count * proc.frame->slice[slice].direct_quality_loss < 1.0) ?
			 (uint8_t)(0xFF * 5 * proc.frame->slice_count * proc.frame->slice[slice].direct_quality_loss) :
			 0xFF);
			int start_x = mb_x << mb_size_log;
			int start_y = mb_y << mb_size_log;
			int end_x = (mb_x + 1) << mb_size_log;
			int end_y = (mb_y + 1) << mb_size_log;
			/* Y */
			if (proc.frame->replacement)
				for (y = start_y; y < end_y; y++)
					for (x = start_x; x < end_x; x += sizeof(byte_block_t))
						BLOCK(frame->data[0], x + y * frame->linesize[0]) = error;
			else
				for (y = start_y; y < end_y; y++)
					for (x = start_x; x < end_x; x += sizeof(byte_block_t))
						BLOCK(frame->data[0], x + y * frame->linesize[0]) = checkerboard(mb_x, mb_y);
			start_x = mb_x << (mb_size_log - 1);
			start_y = mb_y << (mb_size_log - 1);
			end_x = (mb_x + 1) << (mb_size_log - 1);
			end_y = (mb_y + 1) << (mb_size_log - 1);
			/* Cb */
			for (y = start_y; y < end_y; y++)
				for (x = start_x; x < end_x; x += sizeof(byte_block_t))
					BLOCK(frame->data[1], x + y * frame->linesize[1]) = byte_spread * 0x80;
			/* Cr */
			for (y = start_y; y < end_y; y++)
				for (x = start_x; x < end_x; x += sizeof(byte_block_t))
					BLOCK(frame->data[2], x + y * frame->linesize[2]) = byte_spread * 0x80;
		}
		
		/* write slice number onto the image */
		print_number(frame, proc.frame->slice[slice].start_index, proc.frame->slice[slice].end_index);
	}
}

static void draw_border(const replacement_node_t *node, AVPicture *frame)
{
	int x, y;
	if (!node) return;
	if (node->node[0]) {
		draw_border(node->node[0], frame);
		draw_border(node->node[1], frame);
		draw_border(node->node[2], frame);
		draw_border(node->node[3], frame);
	} else {
		for (x = (node->start_x << mb_size_log); x < (node->end_x << mb_size_log); x++)
			frame->data[0][x + (node->start_y << mb_size_log) * frame->linesize[0]] ^= 0x80;
		for (y = (node->start_y << mb_size_log) + 1; y < (node->end_y << mb_size_log); y++)
			frame->data[0][(node->start_x << mb_size_log) + y * frame->linesize[0]] ^= 0x80;
	}
}

#pragma mark -


#pragma mark Hook Implementations

void hook_slice_end(const AVCodecContext *c)
{
	/* round width and height to integer macroblock multiples */
	const int width  = (2 * (c->width  + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	const int height = (2 * (c->height + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	AVPicture quad;
	
	if (c->slice.flag_last) {
		/* use per-frame storage to keep the replaced image */
		avpicture_fill(&quad, private_data(c->frame.current), c->pix_fmt, width, height);
		/* left upper quadrant: original */
		av_picture_copy(&quad, (const AVPicture *)c->frame.current, PIX_FMT_YUV420P, c->width, c->height);
		/* right upper quadrant: replacement */
		quad.data[0] += c->width;
		quad.data[1] += c->width / 2;
		quad.data[2] += c->width / 2;
		do_replacement(c, &quad, SLICE_MAX, NULL);
		/* left lower quadrant: slice error map */
		quad.data[0] += (c->height    ) * quad.linesize[0] - (c->width    );
		quad.data[1] += (c->height / 2) * quad.linesize[1] - (c->width / 2);
		quad.data[2] += (c->height / 2) * quad.linesize[2] - (c->width / 2);
		draw_slice_error(&quad);
		/* right lower quadrant: replacement with borders */
		quad.data[0] += c->width;
		quad.data[1] += c->width / 2;
		quad.data[2] += c->width / 2;
		do_replacement(c, &quad, SLICE_MAX, NULL);
		draw_border(proc.frame->replacement, &quad);
	}
}

void hook_frame_end(const AVCodecContext *c)
{
	/* round width and height to integer macroblock multiples */
	const int width  = (2 * (c->width  + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	const int height = (2 * (c->height + ((1 << mb_size_log) - 1)) >> mb_size_log) << mb_size_log;
	AVPicture quad;
	
	if (c->frame.display) {
		/* dump the current display image and its stored replacement to disk */
		int y;
		static FILE *vis = NULL;
		if (!vis) vis = fopen("Visualization.yuv", "w");
		avpicture_fill(&quad, private_data(c->frame.display), c->pix_fmt, width, height);
		/* Y */
		for (y = 0; y < 2 * c->height; y++)
			fwrite(quad.data[0] + y * quad.linesize[0], 1, 2 * c->width, vis);
		/* Cb */
		for (y = 0; y < c->height; y++)
			fwrite(quad.data[1] + y * quad.linesize[1], 1, c->width, vis);
		/* Cr */
		for (y = 0; y < c->height; y++)
			fwrite(quad.data[2] + y * quad.linesize[2], 1, c->width, vis);
	}
}

/*
 * Copyright (C) 2006-2010 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "process.h"

static void process_slice(AVCodecContext *c);
#if SIDEBAND_READ
static void process_sideband(const uint8_t *);
#endif
#if SIDEBAND_WRITE
static void write_sideband_data(void);
#endif
static void resize_storage(int mb_width, int mb_height);
static void setup_frame(const AVCodecContext *c);
static void destroy_frames_list(void);

struct proc_s proc = {
last_idr: NULL, frame: NULL,
#if SIDEBAND_READ
lookahead: NULL,
#endif
#ifdef SCHEDULE_EXECUTE
propagation: { vis_frame: { data: { NULL, NULL, NULL, NULL } }, total_error: 0.0 },
#endif
#if LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || LLSP_PREDICT
llsp: { decode: NULL, replace: NULL },
#endif
#if LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE
llsp: { train_coeffs: "Coefficients.dat" },
#endif
#if LLSP_PREDICT
llsp: { predict_coeffs: "Coefficients.dat" },
#endif
#ifdef SCHEDULE_EXECUTE
schedule: { first_to_drop: -1 },
#endif
#if PREPROCESS || LLSP_TRAIN_REPLACE
temp_frame: { data: { NULL, NULL, NULL, NULL } },
#endif
mb_width: -1, mb_height: -1
};


void process_init(AVCodecContext *c, char *file)
{
	// FIXME: we need to be single-threaded for now
	c->thread_count = 1;
	memset(&c->metrics,   0, sizeof(c->metrics  ));
	memset(&c->timing ,   0, sizeof(c->timing   ));
	memset(&c->slice,     0, sizeof(c->slice    ));
	memset(&c->reference, 0, sizeof(c->reference));
	memset(&c->frame,     0, sizeof(c->frame    ));
	c->process_slice = (void (*)(void *))process_slice;
#if SIDEBAND_READ
	c->process_sideband = process_sideband;
#else
	c->process_sideband = NULL;
#endif
	if (frame_storage_alloc)
		c->get_buffer = frame_storage_alloc;
	if (frame_storage_destroy)
		c->release_buffer = frame_storage_destroy;
	srandom(0);
#if SIDEBAND_WRITE
	if (strcmp(&file[strlen(file) - sizeof(".h264") + 1], ".h264") == 0) {
		proc.sideband.from = fopen(file, "r");
		file[strlen(file) - sizeof("h264") + 1] = 'p';
		proc.sideband.to   = fopen(file, "w");
	} else {
		printf("filename does not have the proper .h264 ending\n");
		exit(1);
	}
#endif
#if (SIDEBAND_WRITE && PREPROCESS) || (SIDEBAND_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME))
	if (strcmp(&file[strlen(file) - sizeof(".p264") + 1], ".p264") == 0) {
		file[strlen(file) - sizeof("p264") + 2] = 'r';
		file[strlen(file) - sizeof("p264") + 3] = 'o';
		file[strlen(file) - sizeof("p264") + 4] = 'p';
		proc.sideband.propagation = fopen(file,
#if SIDEBAND_READ
										  "r"
#else
										  "w"
#endif
										  );
	} else {
		printf("filename does not have the proper .p264 ending\n");
		exit(1);
	}
#endif
#if LLSP_TRAIN_DECODE || LLSP_PREDICT
	if (!proc.llsp.decode)
		proc.llsp.decode  = llsp_new(LLSP_DECODE, METRICS_COUNT);
#endif
#if LLSP_TRAIN_REPLACE || LLSP_PREDICT
	if (!proc.llsp.replace)
		proc.llsp.replace = llsp_new(LLSP_REPLACE, 1);
#endif
#if LLSP_PREDICT
	if (proc.llsp.predict_coeffs) {
		llsp_load(proc.llsp.decode,  proc.llsp.predict_coeffs);
		llsp_load(proc.llsp.replace, proc.llsp.predict_coeffs);
	}
#endif
	FFMPEG_TIME_START(c, total);
}

void process_finish(AVCodecContext *c)
{
#if SIDEBAND_WRITE
	int bytes;
#endif
#if PREPROCESS
	accumulate_quality_loss(proc.last_idr);
#endif
#if SIDEBAND_WRITE
	/* flush remaining frames */
	write_sideband_data();
	/* copy the remainder of the file */
	do {
		bytes = fread(proc.sideband.buf, 1, sizeof(proc.sideband.buf), proc.sideband.from);
		fwrite(proc.sideband.buf, 1, bytes, proc.sideband.to);
	} while (bytes);
	fclose(proc.sideband.to);
	fclose(proc.sideband.from);
#endif
#if (SIDEBAND_WRITE && PREPROCESS) || (SIDEBAND_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME))
	if (proc.sideband.propagation) fclose(proc.sideband.propagation);
#endif
	while (proc.last_idr)
		destroy_frames_list();
#if SIDEBAND_READ
	proc.lookahead = NULL;
#endif
#if LLSP_TRAIN_DECODE
	if (proc.llsp.train_coeffs && llsp_finalize(proc.llsp.decode))
		llsp_store(proc.llsp.decode,  proc.llsp.train_coeffs);
#endif
#if LLSP_TRAIN_REPLACE
	if (proc.llsp.train_coeffs && llsp_finalize(proc.llsp.replace))
		llsp_store(proc.llsp.replace, proc.llsp.train_coeffs);
#endif
#if LLSP_PREDICT && !LLSP_TRAIN_DECODE && !LLSP_TRAIN_REPLACE
	/* we cannot free the handles during training, since we may want to train on multiple files */
	llsp_dispose(proc.llsp.decode);
	llsp_dispose(proc.llsp.replace);
#endif
#ifdef SCHEDULE_EXECUTE
	printf("%lf\n", proc.propagation.total_error);
#endif
}

static void process_slice(AVCodecContext *c)
{
	int skip_slice = 0;
	
	FFMPEG_TIME_STOP(c, total);
	if (hook_slice_any) hook_slice_any(c);
	
	switch (c->metrics.type) {
		case PSEUDO_SLICE_FRAME_START:
			/* pseudo slice at the start of the frame, before the first real slice */
			if (c->frame.flag_idr) {
				/* flush frame list on IDR */
#if PREPROCESS
				accumulate_quality_loss(proc.last_idr);
#endif
#if SIDEBAND_WRITE
				write_sideband_data();
#endif
				destroy_frames_list();
			}
			resize_storage(c->frame.mb_width, c->frame.mb_height);
			setup_frame(c);
#if SLICE_SKIP
			skip_slice = perform_slice_skip(c);
#endif
			break;
			
		case PSEUDO_SLICE_FRAME_END:
			/* pseudo slice at the end of the frame, after the last real slice finished */
			if (!proc.frame) break;
			if (hook_frame_end) hook_frame_end(c);
#ifdef SCHEDULE_EXECUTE
			propagation_visualize(c);
#endif
			break;
			
		default:
			/* regular slice */
			if (!proc.frame) break;
#if METRICS_EXTRACT
			remember_metrics(c);
#endif
#if LLSP_TRAIN_DECODE || defined(LLSP_PREDICTION)
			proc.llsp.decoding_time += get_time();
#endif
#if LLSP_TRAIN_DECODE
			if (proc.llsp.train_coeffs)
				llsp_accumulate(proc.llsp.decode, metrics_decode(proc.frame, proc.frame->slice_count), proc.llsp.decoding_time);
#endif
#ifdef LLSP_PREDICTION
			printf("%lf, %lf\n", proc.frame->slice[proc.frame->slice_count].decoding_time, proc.llsp.decoding_time);
#endif
#if PREPROCESS
			remember_slice_boundaries(c);
			remember_reference_frames(c);
			remember_dependencies(c);
#endif
#if !SIDEBAND_READ || METRICS_EXTRACT || PREPROCESS || LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || defined(LLSP_PREDICTION)
			if (++proc.frame->slice_count == SLICE_MAX) {
				printf("ERROR: maximum number of slices exceeded\n");
				exit(1);
			}
#endif
			if (c->slice.flag_last) {
#if PREPROCESS
				search_replacements(c, proc.frame->replacement);
#endif
				if (hook_slice_end) hook_slice_end(c);
#if LLSP_TRAIN_REPLACE
				replacement_time(c);
#endif
			}
#if SLICE_SKIP
			skip_slice = perform_slice_skip(c);
#endif
	}
	
	memset(&c->metrics,   0, sizeof(c->metrics  ));
	memset(&c->timing ,   0, sizeof(c->timing   ));
	memset(&c->slice,     0, sizeof(c->slice    ));
	
	/* pass skipping hint to FFmpeg so it can drop the decoding of the upcoming slice */
	c->slice.skip    = skip_slice;
#ifdef SCHEDULE_EXECUTE
	c->slice.conceal = proc.schedule.conceal;
#endif
	
	FFMPEG_TIME_START(c, total);
#if LLSP_TRAIN_DECODE || defined(LLSP_PREDICTION)
	proc.llsp.decoding_time = -get_time();
#endif
}

#if SIDEBAND_READ
static void process_sideband(const uint8_t *nalu)
{
	frame_node_t *frame;
	uint16_t mb_width, mb_height;
	int i;
	
	frame = (frame_node_t *)av_malloc(sizeof(frame_node_t));
	if (!proc.last_idr)
		proc.last_idr = frame;
	if (proc.lookahead)
		proc.lookahead->next = frame;
	proc.lookahead = frame;
	proc.lookahead->next = NULL;
	
	nalu_read_start(nalu);
	mb_width  = nalu_read_uint16();
	mb_height = nalu_read_uint16();
	resize_storage(mb_width, mb_height);
	frame->slice_count = nalu_read_uint8();
	for (i = 0; i < frame->slice_count; i++)
		read_metrics(frame, i);
	read_replacement_tree(NULL);
	for (i = 0; i < frame->slice_count; i++) {
		frame->slice[i].start_index = nalu_read_uint16();
		if (i > 0)
			frame->slice[i-1].end_index = frame->slice[i].start_index;
		if (i == frame->slice_count - 1)
			frame->slice[i].end_index = proc.mb_width * proc.mb_height;
		if (frame->replacement)
			frame->slice[i].direct_quality_loss = nalu_read_float();
	}
	for (i = 0; i < frame->slice_count; i++)
		frame->slice[i].emission_factor = nalu_read_float();
	
#if SIDEBAND_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME)
	/* read immission factors for slice tracking from separate file */
	read_immission(frame);
#endif
	
#if LLSP_PREDICT
	for (i = 0; i < frame->slice_count; i++) {
		/* calculate the benefit value for each slice */
		frame->slice[i].decoding_time    = llsp_predict(proc.llsp.decode, metrics_decode(frame, i));
		frame->slice[i].replacement_time = llsp_predict(proc.llsp.replace, metrics_replace(frame, i));
		if (!frame->replacement || frame->slice[i].decoding_time <= frame->slice[i].replacement_time)
			frame->slice[i].benefit = HUGE_VAL;
		else
#if SCHEDULING_METHOD == COST
			frame->slice[i].benefit = 1 / frame->slice[i].decoding_time;
#elif SCHEDULING_METHOD == DIRECT_ERROR
		frame->slice[i].benefit = frame->slice[i].direct_quality_loss;
#elif SCHEDULING_METHOD == LIFETIME
		frame->slice[i].benefit =
		(frame->slice[i].direct_quality_loss * (1 + frame->reference_lifetime)) /
		(frame->slice[i].decoding_time - frame->slice[i].replacement_time);
#else
		frame->slice[i].benefit =
		(frame->slice[i].direct_quality_loss * frame->slice[i].emission_factor) /
		(frame->slice[i].decoding_time - frame->slice[i].replacement_time);
#endif
		/* safety margin */
		frame->slice[i].decoding_time    *= safety_margin_decode;
		frame->slice[i].replacement_time *= safety_margin_replace;
	}
#endif
}
#endif

#if SIDEBAND_WRITE
static void write_sideband_data(void)
{
	static int lookahead_left = frame_lookahead;
	frame_node_t *frame;
	int i;
	
	if (!proc.frame) return;
	/* process frame list up to and including proc.frame */
	for (frame = proc.last_idr; frame != proc.frame->next; frame = frame->next) {
		/* align the stream to the next slice start */
		while (!slice_start()) copy_nalu();
		
		/* write our sideband data as a custom NALU */
		nalu_write_start();
		nalu_write_uint16(proc.mb_width);
		nalu_write_uint16(proc.mb_height);
		nalu_write_uint8(frame->slice_count);
#if METRICS_EXTRACT || SIDEBAND_READ
		for (i = 0; i < frame->slice_count; i++)
			write_metrics(frame, i);
#else
		for (i = 0; i < (1 + 13 * 3) * frame->slice_count; i++)
			nalu_write_uint8(0);
#endif
#if PREPROCESS || SIDEBAND_READ
		write_replacement_tree(frame->replacement);
		for (i = 0; i < frame->slice_count; i++) {
			nalu_write_uint16(frame->slice[i].start_index);
			if (frame->replacement)
				nalu_write_float(frame->slice[i].direct_quality_loss);
		}
#else
		nalu_write_uint16(0);  /* empty replacement tree */
		nalu_write_uint16(0);  /* first slice's start_index */
		for (i = 0; i < frame->slice_count - 1; i++)
			nalu_write_uint16(proc.mb_width * proc.mb_height);
#endif
		for (i = 0; i < frame->slice_count; i++)
#if PREPROCESS || SIDEBAND_READ
			nalu_write_float(frame->slice[i].emission_factor);
#else
		nalu_write_float(0.0);
#endif
		nalu_write_end();
		
#if SIDEBAND_WRITE && PREPROCESS && !SIDEBAND_READ
		/* store immission factors in a separate file, so we can use them for slice tracking later */
		write_immission(frame);
#endif
		
		if (!lookahead_left)
		/* copy slices worth a full frame */
			for (i = 0; i < frame->slice_count; i++) {
				copy_nalu();
				/* skip to the next slice start */
				while (!slice_start()) copy_nalu();
			}
		else
			lookahead_left--;
	}
}
#endif

static void resize_storage(int mb_width, int mb_height)
{
	if (mb_width != proc.mb_width || mb_height != proc.mb_height) {
#if PREPROCESS || LLSP_TRAIN_REPLACE
		avpicture_free(&proc.temp_frame);
		avpicture_alloc(&proc.temp_frame, PIX_FMT_YUV420P, mb_width << mb_size_log, mb_height << mb_size_log);
#endif
#if PREPROCESS
		av_free(proc.ref_num[0]);
		av_free(proc.ref_num[1]);
		proc.ref_num[0] = (int8_t *)av_malloc((2*mb_width+1) * 2*mb_height * sizeof(int8_t));
		proc.ref_num[1] = (int8_t *)av_malloc((2*mb_width+1) * 2*mb_height * sizeof(int8_t));
#endif
#ifdef SCHEDULE_EXECUTE
		avpicture_free(&proc.propagation.vis_frame);
		avpicture_alloc(&proc.propagation.vis_frame, PIX_FMT_YUV420P,
						mb_width << (mb_size_log + 1), mb_height << (mb_size_log + 1));
#endif
		proc.mb_width  = mb_width;
		proc.mb_height = mb_height;
	}
}

static void setup_frame(const AVCodecContext *c)
{
#if PREPROCESS
	int slice;
#endif
	
#if SIDEBAND_READ
	/* frame node has already been allocated by sideband lookahead */
	if (!proc.frame)
		proc.frame = proc.last_idr;
	else if (proc.frame->next)
		proc.frame = proc.frame->next;
	else
    /* we fell off the frames list, bad thing */
		destroy_frames_list();
	if (!proc.frame) return;
#else
	frame_node_t *frame;
	
	/* allocate new frame node */
	frame = (frame_node_t *)av_malloc(sizeof(frame_node_t));
	if (!proc.last_idr)
		proc.last_idr = frame;
	if (proc.frame)
		proc.frame->next = frame;
	proc.frame = frame;
	/* initialize */
	proc.frame->next = NULL;
#endif
	
#if PREPROCESS && SIDEBAND_READ
	/* lookahead already created a replacement tree, which we want to re-create from scratch */
	destroy_replacement_tree(proc.frame->replacement);
#endif
#if PREPROCESS || SLICE_SKIP
	/* SLICE_MAX is a "virtual" slice that covers the entire frame;
	 * this allows handling a full frame with one do_replacement() call;
	 * quite a hack, I know... */
	proc.frame->slice[SLICE_MAX].start_index = 0;
	proc.frame->slice[SLICE_MAX].end_index = proc.mb_width * proc.mb_height;
#endif
#if PREPROCESS
	proc.frame->slice[SLICE_MAX].rect.min_x = 0;
	proc.frame->slice[SLICE_MAX].rect.min_y = 0;
	proc.frame->slice[SLICE_MAX].rect.max_x = c->width;
	proc.frame->slice[SLICE_MAX].rect.max_y = c->height;
	proc.frame->replacement = NULL;
#endif
#if PREPROCESS
	for (slice = 0; slice < SLICE_MAX + 1; slice++) {
		memset(proc.frame->slice[slice].immission_base, 0, sizeof(proc.frame->slice[0].immission_base));
		proc.frame->slice[slice].immission = proc.frame->slice[slice].immission_base + REF_MAX;
	}
	proc.frame->reference = proc.frame->reference_base + REF_MAX;
#endif
	
#if !SIDEBAND_READ || METRICS_EXTRACT || PREPROCESS || LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || defined(LLSP_PREDICTION)
	proc.frame->slice_count = 0;
#endif
#if PREPROCESS
	/* attach the frame node to the frame so it travels nicely through FFmpeg,
	 * opaque needs to be a double-pointer, because we are modifying a copy of the actual
	 * AVFrame here; so modifying a direct pointer would have no effect on the original */
	*(frame_node_t **)c->frame.current->opaque = proc.frame;
	/* the frame must be unused by both FFmpeg and ourselves */
	proc.frame->reference_count = 2;
#endif
}

static void destroy_frames_list(void)
{
	frame_node_t *frame, *prev;
	
	if (!proc.frame) return;
	/* destroy frame list up to and including proc.frame */
	for (frame = proc.last_idr, prev = NULL; frame != proc.frame->next; prev = frame, frame = frame->next) {
#if PREPROCESS || SIDEBAND_READ
		destroy_replacement_tree(frame->replacement);
		frame->replacement = NULL;
#endif
		if (prev
#if PREPROCESS
			&& !--prev->reference_count
#endif
			)
			av_free(prev);
	}
	proc.last_idr = proc.frame->next;
	proc.frame = NULL;
	if (prev
#if PREPROCESS
		&& !--prev->reference_count
#endif
		)
		av_free(prev);
}

#if LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || defined(LLSP_PREDICTION) || defined(FINAL_SCHEDULING)
double get_time(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (double)time.tv_sec + (double)time.tv_usec / 1E6;
}
#endif

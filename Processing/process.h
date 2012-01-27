/*
 * Copyright (C) 2006-2011 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "config.h"
#define HAVE_AV_CONFIG_H

#include "libavcodec/avcodec.h"

/* FFmpeg wants to disable those, but I want to use them */
#undef printf
#undef fprintf
#undef exit

#if LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || LLSP_PREDICT || defined(LLSP_SUPPORT)
#  include "llsp.h"
#  define LLSP_SUPPORT 1
#else
#  define LLSP_SUPPORT 0
#endif

#if PREPROCESS || SLICE_SKIP
#  include "ssim.h"
#endif

#pragma mark -


#pragma mark Workbench Configuration

/* The H.264 processing workbench is highly configurable with the defines
 * below. Every possible combination should compile and work, although some
 * don't make much sense, but then you will get a compile-time warning.
 * Unfortunately, this leads to quite some preprocessor hell, but you'll
 * get used to it. */

/* toggle extraction and storage of decoding time metrics */
#ifdef METRICS_EXTRACT
#  define METRICS_EXTRACT      1
#else
#  define METRICS_EXTRACT      0
#endif

/* toggle video preprocessing (replacement and slice interdependencies) */
#ifdef PREPROCESS
#  define PREPROCESS           1
#else
#  define PREPROCESS           0
#endif

/* toggle writing of sideband-data-enriched H.264 file */
#ifdef SIDEBAND_WRITE
#  define SIDEBAND_WRITE       1
#else
#  define SIDEBAND_WRITE       0
#endif

/* toggle parsing of sideband data from preprocessed H.264 file */
#ifdef SIDEBAND_READ
#  define SIDEBAND_READ        1
#else
#  define SIDEBAND_READ        0
#endif

/* toggle slice skipping and replacement based on some scheduling */
#ifdef SLICE_SKIP
#  define SLICE_SKIP           1
#else
#  define SLICE_SKIP           0
#endif

/* toggle training the decoding time prediction */
#ifdef LLSP_TRAIN_DECODE
#  define LLSP_TRAIN_DECODE    1
#else
#  define LLSP_TRAIN_DECODE    0
#endif

/* toggle training the replacement time prediction */
#ifdef LLSP_TRAIN_REPLACE
#  define LLSP_TRAIN_REPLACE   1
#else
#  define LLSP_TRAIN_REPLACE   0
#endif

/* toggle execution time predictions */
#ifdef LLSP_PREDICT
#  define LLSP_PREDICT         1
#else
#  define LLSP_PREDICT         0
#endif

/* configuration presets */
#if defined(FINAL_SCHEDULING) || \
defined(SCHEDULE_EXECUTE)
#undef METRICS_EXTRACT
#undef PREPROCESS
#undef PREPROCESS
#undef SIDEBAND_WRITE
#undef SIDEBAND_READ
#undef SLICE_SKIP
#undef LLSP_TRAIN
#undef LLSP_PREDICT
#endif
#ifdef FINAL_SCHEDULING
#define METRICS_EXTRACT		0
#define PREPROCESS		0
#define PREPROCESS		0
#define SIDEBAND_WRITE		0
#define SIDEBAND_READ		1
#define SLICE_SKIP		1
#define LLSP_TRAIN		0
#define LLSP_PREDICT		1
#endif
#ifdef SCHEDULE_EXECUTE
#define METRICS_EXTRACT		0
#define PREPROCESS		0
#define PREPROCESS		0
#define SIDEBAND_WRITE		0
#define SIDEBAND_READ		1
#define SLICE_SKIP		1
#define LLSP_TRAIN		0
#define LLSP_PREDICT		0
#endif

/* configuration dependencies */
#if METRICS_EXTRACT && !FFMPEG_METRICS
#  warning  FFmpeg is not configured correctly, check avcodec.h
#endif
#if SIDEBAND_WRITE && !METRICS_EXTRACT
#  warning  extracted sideband data might be invalid
#endif
#if SLICE_SKIP && !SIDEBAND_READ
#  warning  slice skipping only works properly with sideband data available
#endif
#if LLSP_PREDICT && !SIDEBAND_READ
#  warning  execution time prediction only works properly with sideband data available
#endif
#if (LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE) && !(SIDEBAND_READ || (METRICS_EXTRACT && PREPROCESS))
#  warning  training the predictor requires metrics and slice boundaries
#endif
#if SLICE_SKIP && (METRICS_EXTRACT || PREPROCESS)
#  warning  slices will not be skipped for real as this would scramble the extracted sideband data
#endif

/* maximum supported number of slices per frame, must be less than 256 */
#define SLICE_MAX 32
/* maximum supported number of references per frame, must be less than 128 */
#define REF_MAX 32

/* scheduling methods */
#define COST		1
#define DIRECT_ERROR	2
#define NO_SKIP		3
#define LIFETIME	4

#if LLSP_SUPPORT
/* number of decoding time metrics in use */
#define METRICS_COUNT 12
/* the LLSP solver IDs */
enum { LLSP_DECODE, LLSP_REPLACE };
#endif

#pragma mark -


#pragma mark Hooks for Tools

#ifndef IMPLEMENTS_HOOKS
#  define WEAK_SYMBOL __attribute__((weak))
#else
#  define WEAK_SYMBOL
#endif

extern int frame_storage_alloc(AVCodecContext *c, AVFrame *frame) WEAK_SYMBOL;
extern void frame_storage_destroy(AVCodecContext *c, AVFrame *frame) WEAK_SYMBOL;
extern void hook_slice_any(const AVCodecContext *c) WEAK_SYMBOL;
extern void hook_slice_end(const AVCodecContext *c) WEAK_SYMBOL;
extern void hook_frame_end(const AVCodecContext *c) WEAK_SYMBOL;

#pragma mark -


#pragma mark Type Definitions

typedef struct replacement_node_s replacement_node_t;
typedef struct frame_node_s frame_node_t;
typedef float propagation_t[SLICE_MAX];
#if !PREPROCESS
typedef void change_rect_t;
#endif

/* node in the quadtree describing frame replacement */
struct replacement_node_s {
	/* position of this node in the tree */
	unsigned depth, index;
	/* block boundaries (start inclusive, end exclusive) in macroblock coordinates */
	unsigned start_x, start_y, end_x, end_y;
	/* positive: short term, negative: long term; invariant: never zero */
	int reference;
	/* offset of the replacement */
	int x, y;
	/* subnodes; invariant: either all four or none exist */
	replacement_node_t *node[4];
};

/* storage for per-frame data */
struct frame_node_s {
#if PREPROCESS
	/* avoids premature deletion */
	int reference_count;
#endif
	
	int slice_count;
	/* storage for per-slice data, slice[SLICE_MAX] is a pseudo-slice covering the whole frame */
	struct {
		/* decoding time metrics */
#if METRICS_EXTRACT || SIDEBAND_READ
		struct {
			int type, bits;
			int intra_pcm, intra_4x4, intra_8x8, intra_16x16;
			int inter_4x4, inter_8x8, inter_16x16;
			int idct_4x4, idct_8x8;
			int deblock_edges;
		} metrics;
#endif
#if PREPROCESS
		/* rectangle around the slice, min inclusive, max exclusive */
		change_rect_t rect;
#endif
#if PREPROCESS || SIDEBAND_READ
		/* macroblock indices for the slice, start inclusive, end exclusive */
		int start_index, end_index;
		/* measured quality loss due to replacement */
		float direct_quality_loss;
		/* how much does an error from this slice propagate into future slices */
		float emission_factor;
#endif
#if PREPROCESS || (SCHEDULING_METHOD == LIFETIME)
		/* how much do errors from reference slices propagate into this slice */
		propagation_t immission_base[2 * REF_MAX + 1];
		/* propagation will point to propagation_base[REF_MAX], so it can be indexed
		 * from -REF_MAX to REF_MAX, which is the values range of reference numbers */
		propagation_t *immission;
#endif
#if LLSP_PREDICT
		/* estimated execution times */
		float decoding_time, replacement_time;
		/* benefit when this slice is decoded */
		float benefit;
#endif
#if defined(FINAL_SCHEDULING)
		/* is this slice to be skipped */
		int skip;
#endif
	} slice[SLICE_MAX + 1];
	
#if PREPROCESS || SIDEBAND_READ
	/* the root of the replacement quadtree, NULL if no replacement is possible */
	replacement_node_t *replacement;
#endif
#if PREPROCESS
	/* a copy of the reference stack for this frame */
	frame_node_t *reference_base[2 * REF_MAX + 1];
	/* reference will point to reference_base[REF_MAX], so it can be indexed
	 * from -REF_MAX to REF_MAX, which is the values range of reference numbers */
	frame_node_t **reference;
#endif
#if PREPROCESS || (SCHEDULING_METHOD == LIFETIME)
	/* how many future frames will see this frame in the reference stacks */
	int reference_lifetime;
#endif
	/* the next frame in decoding order */
	frame_node_t *next;
};

extern struct proc_s {
	/* frames nodes are always kept from one IDR frame to the next, because
	 * some processing like backwards-accumulation of error propagation can only
	 * be performed at IDR frames; the layout of the frames list is:
	 * 
	 * node -> node -> node -> node -> node -> node -> node -> node -> node
	 *  ^                       ^                                       ^
	 * last_idr                frame                                 lookahead
	 * 
	 * If we are not currently reading sideband data, the list ends at the "frame"
	 * node and lookahead does not exist. If we are reading sideband data, the
	 * distance between "frame" and "lookahead" is constant (see frame_lookahead).
	 * 
	 * If the upcoming frame is an IDR, the list should be processed from last_idr
	 * to frame->next. This will mark the end regardless of sideband reading. */
	
	/* anchor of the frame list */
	frame_node_t *last_idr;
	/* current frame */
	frame_node_t *frame;
#if SIDEBAND_READ
	/* sideband data already read */
	frame_node_t *lookahead;
#endif
	
	/* state variables for sideband reading/writing */
	struct {
#if SIDEBAND_WRITE
		FILE *from, *to;
		uint8_t buf[4096];
		uint8_t history[3];
		uint32_t remain;
		uint8_t remain_bits;
#endif
#if SIDEBAND_READ
		const uint8_t * restrict nalu;
		uint8_t byte;
		uint8_t bits;
#endif
#if (SIDEBAND_WRITE && PREPROCESS) || (SIDEBAND_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME))
		/* the immission factors do not belong to the sideband data, but some of our
		 * visualizations and measurements need them, so store them in an extra file */
		FILE *propagation;
#endif
	} sideband;
	
#ifdef SCHEDULE_EXECUTE
	/* just some helpers for error propagation visualization */
	struct {
		AVPicture vis_frame;
		double total_error;
	} propagation;
#endif
	
#if LLSP_TRAIN_DECODE || LLSP_TRAIN_REPLACE || LLSP_PREDICT
	struct {
		const char *train_coeffs;
		const char *predict_coeffs;
		llsp_t *decode;
		llsp_t *replace;
		double decoding_time;
	} llsp;
#endif
	
#ifdef SCHEDULE_EXECUTE
	struct {
		int conceal;
		int first_to_drop;
	} schedule;
#endif
	/* current video size */
	int mb_width, mb_height;
#if PREPROCESS || LLSP_TRAIN_REPLACE
	/* intermediary frame storage */
	AVPicture temp_frame;
#endif
#if PREPROCESS
	/* translated reference numbers (slice-local to global) of the current frame */
	int8_t *ref_num[2];
#endif
} proc;

static const int mb_size_log = 4;	/* log2 of the edge length of a macroblock */
static const float ssim_precision = 0.05;
static const float subdivision_threshold = 0.01;
static const int frame_lookahead = 25;
static const int output_queue = 10;     /* length of simulated player's frame queue */
#if SLICE_SKIP
static const float safety_margin_decode  = 1.1;
static const float safety_margin_replace = 1.2;
#else
static const float safety_margin_decode  = 1.0;
static const float safety_margin_replace = 1.0;
#endif

#pragma mark -


#pragma mark Functions in process.c

/* this is where it all begins */
void process_init(AVCodecContext *c, char *file);
void process_finish(AVCodecContext *c);
#if LLSP_SUPPORT || defined(FINAL_SCHEDULING)
double get_time(void);
#endif

#pragma mark -


#pragma mark Functions in metrics.c

/* handling of decoding time metrics */
#if METRICS_EXTRACT
void remember_metrics(const AVCodecContext *c);
#endif
#if SIDEBAND_WRITE && (METRICS_EXTRACT || SIDEBAND_READ)
void write_metrics(const frame_node_t *frame, int slice);
#endif
#if SIDEBAND_READ
void read_metrics(frame_node_t *frame, int slice);
#endif
#if LLSP_SUPPORT
const double *metrics_decode(const frame_node_t *frame, int slice);
const double *metrics_replace(const frame_node_t *frame, int slice);
#endif

#pragma mark -


#pragma mark Functions in schedule.c

/* the slice scheduler */
#if SLICE_SKIP
int perform_slice_skip(const AVCodecContext *c);
int schedule_skip(const AVCodecContext *c, int current_slice);
#endif

#pragma mark -


#pragma mark Functions in replace.c

/* the replacement quadtree */
#if PREPROCESS
void search_replacements(const AVCodecContext *c, replacement_node_t *node);
#endif
#if PREPROCESS || SIDEBAND_READ
void destroy_replacement_tree(replacement_node_t *node);
#endif
#if PREPROCESS
void remember_slice_boundaries(const AVCodecContext *c);
void remember_reference_frames(const AVCodecContext *c);
#endif
#if LLSP_TRAIN_REPLACE
void replacement_time(AVCodecContext *c);
#endif
#if PREPROCESS || SLICE_SKIP || LLSP_TRAIN_REPLACE
float do_replacement(const AVCodecContext *c, const AVPicture *frame, int slice, const change_rect_t *rect);
#endif
#if SIDEBAND_WRITE && (PREPROCESS || SIDEBAND_READ)
void write_replacement_tree(const replacement_node_t *node);
#endif
#if SIDEBAND_READ
void read_replacement_tree(replacement_node_t *node);
#endif

#pragma mark -


#pragma mark Functions in propagate.c

/* error propagation */
#if PREPROCESS
void remember_dependencies(const AVCodecContext *c);
void accumulate_quality_loss(frame_node_t *frame);
#endif
#ifdef SCHEDULE_EXECUTE
void propagation_visualize(const AVCodecContext *c);
#endif
#if SIDEBAND_WRITE && PREPROCESS && !SIDEBAND_READ
void write_immission(const frame_node_t *frame);
#endif
#if SIDEBAND_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME)
void read_immission(frame_node_t *frame);
#endif

#pragma mark -


#pragma mark Functions in nalu.c

/* sideband data read and write */
#if SIDEBAND_WRITE
int slice_start(void);
void copy_nalu(void);
void nalu_write_start(void);
void nalu_write_end(void);
void nalu_write_uint8(uint8_t value);
void nalu_write_uint16(uint16_t value);
void nalu_write_uint24(uint32_t value);
void nalu_write_uint32(uint32_t value);
void nalu_write_int8(int8_t value);
void nalu_write_int16(int16_t value);
void nalu_write_float(float value);
#endif
#if SIDEBAND_READ
void nalu_read_start(const uint8_t *nalu);
uint8_t nalu_read_uint8(void);
uint16_t nalu_read_uint16(void);
uint32_t nalu_read_uint24(void);
uint32_t nalu_read_uint32(void);
int8_t nalu_read_int8(void);
int16_t nalu_read_int16(void);
float nalu_read_float(void);
#endif

#pragma mark -


#pragma mark Helpers

/* for efficient handling of eight bytes at once */
typedef uint64_t byte_block_t;
static const byte_block_t byte_spread = 0x0101010101010101ULL;
#define BLOCK(base, offset) *cast_to_byte_block_pointer(base, offset)
static inline byte_block_t *cast_to_byte_block_pointer(const uint8_t *const restrict base, const int offset)
{
	return (byte_block_t *)(base + offset);
}

/* macroblock checkerboard pattern */
static inline byte_block_t checkerboard(int mb_x, int mb_y)
{
	return ((mb_x & 1) ^ (mb_y & 1)) ? (byte_spread * 0xA0) : (byte_spread * 0x60);
}

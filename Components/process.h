/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include "config.h"
#include "libavcodec/avcodec.h"


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

/* toggle writing of metadata-enriched H.264 file */
#ifdef METADATA_WRITE
#  define METADATA_WRITE       1
#else
#  define METADATA_WRITE       0
#endif

/* toggle parsing of metadata from preprocessed H.264 file */
#ifdef METADATA_READ
#  define METADATA_READ        1
#else
#  define METADATA_READ        0
#endif

/* toggle slice skipping and replacement based on some scheduling */
#ifdef SLICE_SKIP
#  define SLICE_SKIP           1
#else
#  define SLICE_SKIP           0
#endif

/* configuration presets */
#if defined(FINAL_SCHEDULING) || \
defined(SCHEDULE_EXECUTE)
#undef METRICS_EXTRACT
#undef PREPROCESS
#undef PREPROCESS
#undef METADATA_WRITE
#undef METADATA_READ
#undef SLICE_SKIP
#endif
#ifdef FINAL_SCHEDULING
#define METRICS_EXTRACT		0
#define PREPROCESS		0
#define PREPROCESS		0
#define METADATA_WRITE		0
#define METADATA_READ		1
#define SLICE_SKIP		1
#endif
#ifdef SCHEDULE_EXECUTE
#define METRICS_EXTRACT		0
#define PREPROCESS		0
#define PREPROCESS		0
#define METADATA_WRITE		0
#define METADATA_READ		1
#define SLICE_SKIP		1
#endif

/* configuration dependencies */
#if METRICS_EXTRACT && !FFMPEG_METRICS
#  warning  FFmpeg is not configured correctly, check avcodec.h
#endif
#if METADATA_WRITE && !METRICS_EXTRACT
#  warning  extracted metadata will be invalid
#endif
#if SLICE_SKIP && !METADATA_READ
#  warning  slice skipping only works properly with metadata available
#endif
#if SLICE_SKIP && (METRICS_EXTRACT || PREPROCESS)
#  warning  slices will not be skipped for real as this would scramble the extracted metadata
#endif

/* maximum supported number of slices per frame, must be less than 256 */
#define SLICE_MAX 32
/* maximum supported number of references per frame, must be less than 128 */
#define REF_MAX 32

#if METADATA_READ || METADATA_WRITE
#  include "nalu.h"
#endif

#if PREPROCESS || SLICE_SKIP
#  include "ssim.h"
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
#if METRICS_EXTRACT || METADATA_READ
		struct {
			uint32_t type, bits_cabac, bits_cavlc;
			uint32_t intra_4x4, intra_8x8, intra_16x16;
			uint32_t inter_4x4, inter_8x8, inter_16x16;
			uint32_t idct_pcm, idct_4x4, idct_8x8;
			uint32_t deblock_edges;
		} metrics;
#endif
#if PREPROCESS
		/* rectangle around the slice, min inclusive, max exclusive */
		change_rect_t rect;
#endif
#if PREPROCESS || METADATA_READ
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
#if defined(FINAL_SCHEDULING)
		/* is this slice to be skipped */
		int skip;
#endif
	} slice[SLICE_MAX + 1];
	
#if PREPROCESS || METADATA_READ
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
	 * node -> node -> node -> node
	 *  ^                       ^
	 * last_idr                frame
	 * 
	 * If the upcoming frame is an IDR, the list should be processed from last_idr
	 * to frame->next. This will mark the end regardless of metadata reading. */
	
	/* anchor of the frame list */
	frame_node_t *last_idr;
	/* current frame */
	frame_node_t *frame;
	
	/* state variables for metadata reading/writing */
	struct {
#if METADATA_READ
		nalu_read_t *read;
#endif
#if METADATA_WRITE
		nalu_write_t *write;
#endif
#if (METADATA_WRITE && PREPROCESS) || (METADATA_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME))
		/* the immission factors do not belong to the metadata, but some of our
		 * visualizations and measurements need them, so store them in an extra file */
		FILE *propagation;
#endif
	} metadata;
	
#ifdef SCHEDULE_EXECUTE
	/* just some helpers for error propagation visualization */
	struct {
		AVPicture vis_frame;
		double total_error;
	} propagation;
#endif
	
#ifdef SCHEDULE_EXECUTE
	struct {
		int conceal;
		int first_to_drop;
	} schedule;
#endif
	/* current video size */
	int mb_width, mb_height;
#if PREPROCESS
	/* intermediary frame storage */
	AVPicture temp_frame;
	/* translated reference numbers (slice-local to global) of the current frame */
	int8_t *ref_num[2];
#endif
} proc;

static const int mb_size_log = 4;	/* log2 of the edge length of a macroblock */
static const float ssim_precision = 0.05;
static const float subdivision_threshold = 0.01;
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
void process_init(AVCodecContext *c, const char *file);
void process_finish(AVCodecContext *c);

#pragma mark -


#pragma mark Functions in metrics.c

/* handling of decoding time metrics */
#if METRICS_EXTRACT
void remember_metrics(const AVCodecContext *c);
#endif
#if METADATA_WRITE && (METRICS_EXTRACT || METADATA_READ)
void write_metrics(const frame_node_t *frame, int slice);
#endif
#if METADATA_READ
void read_metrics(frame_node_t *frame, int slice);
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
#if PREPROCESS || METADATA_READ
void destroy_replacement_tree(replacement_node_t *node);
#endif
#if PREPROCESS
void remember_slice_boundaries(const AVCodecContext *c);
void remember_reference_frames(const AVCodecContext *c);
#endif
#if PREPROCESS || SLICE_SKIP
float do_replacement(const AVCodecContext *c, const AVPicture *frame, int slice, const change_rect_t *rect);
#endif
#if METADATA_WRITE && (PREPROCESS || METADATA_READ)
void write_replacement_tree(const replacement_node_t *node);
#endif
#if METADATA_READ
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
#if METADATA_WRITE && PREPROCESS && !METADATA_READ
void write_immission(const frame_node_t *frame);
#endif
#if METADATA_READ && !PREPROCESS && (SCHEDULING_METHOD == LIFETIME)
void read_immission(frame_node_t *frame);
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

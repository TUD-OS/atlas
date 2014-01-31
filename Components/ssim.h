/*
 * Copyright (C) 2006-2014 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdint.h>

typedef struct {
	/* rectangle around the area of change, min inclusive, max exclusive */
	uint_fast32_t min_x;
	uint_fast32_t min_y;
	uint_fast32_t max_x;
	uint_fast32_t max_y;
} change_rect_t;

typedef struct {
	/* all relevant data for a picture */
	uint8_t * restrict Y;
	uint8_t * restrict Cb;
	uint8_t * restrict Cr;
	uint_fast32_t line_stride_Y;
	uint_fast32_t line_stride_Cb;
	uint_fast32_t line_stride_Cr;
	uint_fast32_t width;
	uint_fast32_t height;
} picture_t;

/* visualizes SSIM error for an entire image in map,
 * in addition (when hist != NULL) it can accumulate the actual values in hist,
 * this is useful to get error histograms over multiple frames;
 * note that hist is not subject to line_stride */
void ssim_map(const uint8_t * restrict x, const uint8_t * restrict y,
			  uint8_t * restrict map, float * restrict hist,
			  const unsigned width, const unsigned height, const unsigned line_stride);

/* calculates an aggregated quality loss value within given rectangle and with given precision */
float ssim_quality_loss(const picture_t * restrict x, const picture_t * restrict y,
						const change_rect_t * restrict rect, const float precision);

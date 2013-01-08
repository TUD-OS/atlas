/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

typedef struct {
	/* rectangle around the area of change, min inclusive, max exclusive */
	int min_x;
	int min_y;
	int max_x;
	int max_y;
} change_rect_t;

typedef struct {
	/* all relevant data for a picture */
	uint8_t * restrict Y;
	uint8_t * restrict Cb;
	uint8_t * restrict Cr;
	int line_stride_Y;
	int line_stride_Cb;
	int line_stride_Cr;
	int width;
	int height;
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

/*
 * Copyright (C) 2006 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "ssim.h"


static void mse_map(const uint8_t * restrict x, const uint8_t * restrict y,
					uint8_t * restrict map, float * restrict hist,
					unsigned width, unsigned height, unsigned line_stride)
{
	double mse;
	int i, j;
	
#pragma omp parallel for private(j, mse)
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			mse = (double)x[i * line_stride + j] / 255.0 - (double)y[i * line_stride + j] / 255.0;
			mse = mse * mse * 50.0;
			if (hist)
				hist[i * width + j] += mse;
			if (mse <= 0.0)
				map[i * line_stride + j] = 0;
			else if (mse >= 1.0)
				map[i * line_stride + j] = 255;
			else
				map[i * line_stride + j] = 255 * mse;
		}
	}
}

int main(int argc, char **argv)
{
	unsigned width, height;
	unsigned width2, height2;
	FILE *clip1, *clip2, *result;
	uint8_t *buf, *res;
	int i;
	
	if (argc != 2) return 1;
	if (sscanf(argv[1], "%dx%d", &width, &height) != 2) return 1;
	width  /= 2;
	height /= 2;
	width2  = width  / 2;
	height2 = height / 2;
	
	buf = (uint8_t *)malloc(sizeof(uint8_t) * width * height);
	res = (uint8_t *)malloc(sizeof(uint8_t) * width * height * 4);
	if (!buf || !res) return 1;
	
	clip1  = fopen("Clip1.yuv", "r");
	clip2  = fopen("Clip2.yuv", "r");
	result = fopen("Demo.yuv", "w");
	if (!clip1 || !clip2 || !result) return 1;
	
	while (!feof(clip1) || !feof(clip2)) {
		/* Y */
		fread(buf, 1, width * height, clip1);
		for (i = 0; i < height; i++)
			memcpy(res + 2 * i * width, buf + i * width, width);
		fread(buf, 1, width * height, clip2);
		for (i = 0; i < height; i++)
			memcpy(res + (2 * i + 1) * width, buf + i * width, width);
		memset(res + 2 * width * height, 0, 2 * width * height);
		mse_map(res, res + width, res + 2 * width * height, NULL, width, height, 2 * width);
		ssim_map(res, res + width, res + 2 * width * height + width, NULL, width, height, 2 * width);
		fwrite(res, 1, 4 * width * height, result);
		
		/* Cb */
		fread(buf, 1, width2 * height2, clip1);
		for (i = 0; i < height2; i++)
			memcpy(res + 2 * i * width2, buf + i * width2, width2);
		fread(buf, 1, width2 * height2, clip2);
		for (i = 0; i < height2; i++)
			memcpy(res + (2 * i + 1) * width2, buf + i * width2, width2);
		memset(res + 2 * width2 * height2, 0x80, 2 * width2 * height2);
		fwrite(res, 1, 4 * width2 * height2, result);
		
		/* Cr */
		fread(buf, 1, width2 * height2, clip1);
		for (i = 0; i < height2; i++)
			memcpy(res + 2 * i * width2, buf + i * width2, width2);
		fread(buf, 1, width2 * height2, clip2);
		for (i = 0; i < height2; i++)
			memcpy(res + (2 * i + 1) * width2, buf + i * width2, width2);
		memset(res + 2 * width2 * height2, 0x80, 2 * width2 * height2);
		fwrite(res, 1, 4 * width2 * height2, result);
	}
	
	fclose(clip1);
	fclose(clip2);
	fclose(result);
	free(buf);
	free(res);
	
	return 0;
}

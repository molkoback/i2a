/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include "i2a.h"

#include <wand/magick_wand.h>
#include <aalib.h>

#define DEFAULT_TERM_WIDTH_MUL 1.8
#define DEFAULT_BLUR 0.01

static inline void i2a_config_init(struct i2a_config *cfg)
{
	cfg->invert_f = 0;
	cfg->optimize_f = 0;
	cfg->max_width = 0;
	cfg->max_height = 0;
	cfg->term_width_mul = DEFAULT_TERM_WIDTH_MUL;
	cfg->blur = DEFAULT_BLUR;
	cfg->file = NULL;
}

void i2a_context_init(struct i2a_context *ctx)
{
	i2a_config_init(&ctx->cfg);
	ctx->ascii = NULL;
}

/* Changes width and height according to max_width, max_height and
 * term_width_mul. */
static void i2a_resize(
	struct i2a_context *ctx,
	size_t *width,
	size_t *height
)
{
	size_t mw = ctx->cfg.max_width == 0 ? (*width) : ctx->cfg.max_width;
	size_t mh = ctx->cfg.max_height == 0 ? (*height) : ctx->cfg.max_height;
	double ratio = (double)(*width)/(double)(*height);
	if ((double)mw / ratio > (double)mh) {
		*width = (size_t)(mh * ratio);
		*height = mh;
	}
	else {
		*width = mw;
		*height = (size_t)(mw / ratio);
	}
	*width = (size_t)((double)(*width) * ctx->cfg.term_width_mul);
}

/* Converts normalized rgba to grayscale using the luminosity method. */
static inline double rgba_to_gray(
	double r,
	double g,
	double b,
	double a
)
{
	double gray = a * (0.21*r + 0.72*g + 0.07*b);
	if (gray < 0.0) {
		gray = 0.0;
	}
	else if (gray > 1.0) {
		gray = 1.0;
	}
	return gray;
}

/* Converts PixelWand to AAlib 0-255 scale color. */
static inline int pixel_to_aacolor(const PixelWand *p)
{
	return rgba_to_gray(
		PixelGetRed(p),
		PixelGetGreen(p),
		PixelGetBlue(p),
		PixelGetAlpha(p)
	) * 255;
}

/* Scales MagickWand to AAlib virtual image size, writes it to framebuffer
 * and renders it. */
static void aa_write_wand(
	struct i2a_context *ctx,
	aa_context *aactx,
	MagickWand *wand
)
{
	// Resize to virtual size
	size_t vwidth = aa_imgwidth(aactx);
	size_t vheight = aa_imgheight(aactx);
	MagickResizeImage(
		wand,
		vwidth, vheight,
		LanczosFilter, ctx->cfg.blur
	);
	
	// Write data to AAlib framebuffer
	PixelIterator *it = NewPixelIterator(wand);
	PixelWand **pixels = NULL;
	size_t n_wands;
	for (size_t y = 0; y < vheight; y++) {
		pixels = PixelGetNextIteratorRow(it, &n_wands);
		for (size_t x = 0; x < vwidth; x++) {
			int aacolor = pixel_to_aacolor(pixels[x]);
			if (ctx->cfg.invert_f) {
				aacolor = 255 - aacolor;
			}
			aa_putpixel(aactx, x, y, aacolor);
		}
	}
	DestroyPixelIterator(it);
	
	aa_render(
		aactx,
		&aa_defrenderparams,
		0, 0,
		vwidth, vheight
	);
}

/* Creates ascii mat from MagickWand and saves it to ctx.ascii. */
static void create_ascii(
	struct i2a_context *ctx,
	MagickWand *wand,
	size_t width,
	size_t height
)
{
	// Initialize AAlib with mem_d driver and default parameters
	aa_defparams.width = width;
	aa_defparams.height = height;
	aa_context *aactx = aa_init(&mem_d, &aa_defparams, NULL);
	
	// Write wand to AAlib framebuffer
	aa_write_wand(ctx, aactx, wand);
	
	// Write AAlib text to buffer
	char *text = (char*)aa_text(aactx);
	ctx->ascii = mat_init(width, height);
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			ctx->ascii->data[y][x] = text[y * width + x];
		}
	}
	aa_close(aactx);
}

int i2a_run(struct i2a_context *ctx)
{
	int ret = -1;
	
	// Initialize ImageMagick
	MagickWandGenesis();
	if (IsMagickWandInstantiated() == MagickFalse) {
		fprintf(stderr, "error: couldn't initialize ImageMagick\n");
		return ret;
	}
	
	// Load image file
	MagickWand *wand = NewMagickWand();
	if (MagickReadImage(wand, ctx->cfg.file) == MagickFalse) {
		fprintf(stderr, "error: couldn't load image '%s'\n", ctx->cfg.file);
	}
	else {
		// Create mat
		size_t width = MagickGetImageWidth(wand);
		size_t height = MagickGetImageHeight(wand);
		i2a_resize(ctx, &width, &height);
		create_ascii(ctx, wand, width, height);
		
		// Optimize
		if (ctx->cfg.optimize_f) {
			mat_optimize(ctx->ascii);
		}
		
		ret = 0;
	}
	
	DestroyMagickWand(wand);
	MagickWandTerminus();
	return ret;
}

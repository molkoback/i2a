/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include "mat.h"

#include <wand/magick_wand.h>
#include <aalib.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION "1.1.0"

static const char *usage_str = "usage: i2a [options] <image>\n";
static const char *help_str = (
	"options:\n"
	"  -h                print this help message\n"
	"  -x <int>          maximum width\n"
	"  -y <int>          maximum height\n"
	"  -t <double>       terminal width multiplier\n"
	"  -b <double>       gaussian blur\n"
	"  -o                remove whitespace from the right\n"
	"  -V                print version\n"
);
static const char *version_str = (
	"i2a v"VERSION"\n"
	"Copyright (c) 2017 molko <molkoback@gmail.com>\n"
	"Distributed under WTFPL v2\n"
);

struct config {
	int help_f;
	int version_f;
	int optimize_f;
	size_t max_width;
	size_t max_height;
	double term_width_mul;
	double blur;
	char *file;
};

struct i2a_context {
	struct config cfg;
	struct mat *ascii;
};

static inline void config_init(struct config *cfg)
{
	cfg->help_f = 0;
	cfg->version_f = 0;
	cfg->optimize_f = 0;
	cfg->max_width = 0;
	cfg->max_height = 0;
	cfg->term_width_mul = 1.8;
	cfg->blur = 0.01;
	cfg->file = NULL;
}

static inline void i2a_context_init(struct i2a_context *ctx)
{
	config_init(&ctx->cfg);
	ctx->ascii = NULL;
}

static int args_parse(
	struct config *cfg,
	int argc,
	char *argv[]
)
{
	// Disable getopt error messages
	extern int opterr;
	opterr = 0;
	
	int c;
	while ((c = getopt(argc, argv, "hVx:y:t:b:o")) != -1) {
		switch(c) {
		case 'h':
			cfg->help_f = 1;
			break;
		
		case 'V':
			cfg->version_f = 1;
			break;
		
		case 'o':
			cfg->optimize_f = 1;
			break;
		
		case 'x':
			if ((cfg->max_width = atoi(optarg)) == 0) {
				return -1;
			}
			break;
		
		case 'y':
			if ((cfg->max_height = atoi(optarg)) == 0) {
				return -1;
			}
			break;
		case 't':
			if ((cfg->term_width_mul = atof(optarg)) == 0.0) {
				return -1;
			}
			break;
		case 'b':
			if ((cfg->blur = atof(optarg)) == 0.0) {
				return -1;
			}
			break;
		
		default:
			return -1;
		}
	}
	if (optind < argc) {
		cfg->file = argv[optind];
	} else {
		return -1;
	}
	return 0;
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
	if (gray < 0) {
		gray = 0.0;
	}
	else if (gray > 1) {
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
static void i2a_aa_write_wand(
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
			aa_putpixel(
				aactx,
				x, y,
				pixel_to_aacolor(pixels[x])
			);
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
static void i2a_create_ascii(
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
	i2a_aa_write_wand(ctx, aactx, wand);
	
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
		i2a_create_ascii(ctx, wand, width, height);
		ret = 0;
	}
	
	DestroyMagickWand(wand);
	MagickWandTerminus();
	return ret;
}

int main(
	int argc,
	char *argv[]
)
{
	struct i2a_context ctx;
	i2a_context_init(&ctx);
	
	// Handle command-line arguments
	int err = args_parse(&ctx.cfg, argc, argv);
	
	// Print help/version/usage
	if (ctx.cfg.help_f) {
		fprintf(stdout, "%s\n%s", usage_str, help_str);
		return 0;
	}
	if (ctx.cfg.version_f) {
		fprintf(stdout, "%s", version_str);
		return 0;
	}
	if (err < 0) {
		fprintf(stdout, "%s", usage_str);
		return 1;
	}
	
	// Run the main program
	if (i2a_run(&ctx) < 0) {
		return 1;
	}
	
	// Optimize
	if (ctx.cfg.optimize_f) {
		mat_optimize(ctx.ascii);
	}
	
	// Print ascii
	mat_print(ctx.ascii);
	mat_destroy(ctx.ascii);
	return 0;
}

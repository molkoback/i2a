/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include <wand/magick_wand.h>
#include <aalib.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION "1.0.0"

#define TERM_WIDTH_MUL 1.8
#define RESIZE_BLUR 0.2

static const char *usage_str = "usage: i2a [options] <image>\n";
static const char *help_str = (
	"options:\n"
	"  -h                show this help message\n"
	"  -x <width>        maximum width\n"
	"  -y <height>       maximum height\n"
	"  -o                remove whitespace from the right\n"
	"  -V                print version\n"
);
static const char *version_str = (
	"i2a v"VERSION"\n"
	"Copyright (c) 2017 molko <molkoback@gmail.com>\n"
	"Distributed under WTFPL v2\n"
);

struct config {
	int err_f;
	int help_f;
	int version_f;
	int optimize_f;
	size_t max_width;
	size_t max_height;
	char *file;
};

struct ascii {
	size_t width;
	size_t height;
	char **mat;
};

static void config_init(struct config *cfg)
{
	cfg->err_f = 0;
	cfg->help_f = 0;
	cfg->version_f = 0;
	cfg->optimize_f = 0;
	cfg->max_width = 0;
	cfg->max_height = 0;
	cfg->file = NULL;
}

static void args_parse(
	struct config *cfg,
	int argc,
	char *argv[]
)
{
	// Disable getopt error messages
	extern int opterr;
	opterr = 0;
	
	int c;
	while ((c = getopt(argc, argv, "hVx:y:o")) != -1) {
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
				cfg->err_f = 1;
			}
			break;
		
		case 'y':
			if ((cfg->max_height = atoi(optarg)) == 0) {
				cfg->err_f = 1;
			}
			break;
		
		default:
			cfg->err_f = 1;
		}
	}
	if (optind < argc) {
		cfg->file = argv[optind];
	} else {
		cfg->err_f = 1;
	}
}

/* Because characters aren't square. */
static inline void width_rescale(size_t *width)
{
	*width = (size_t)((double)(*width) * TERM_WIDTH_MUL);
}

/* Changes width and height according to max_width and max_height. */
static void resize_max(
	size_t *width,
	size_t *height,
	size_t max_width,
	size_t max_height
)
{
	max_width = max_width == 0 ? (*width) : max_width;
	max_height = max_height == 0 ? (*height) : max_height;
	double ratio = (double)(*width)/(double)(*height);
	if ((double)max_width / ratio > (double)max_height) {
		*width = (size_t)(max_height * ratio);
		*height = max_height;
	}
	else {
		*width = max_width;
		*height = (size_t)(max_width / ratio);
	}
}

/* Resizes wand using Lanczos. */
static inline void wand_resize(
	MagickWand *mw,
	size_t width,
	size_t height
)
{
	MagickResizeImage(
		mw,
		width, height,
		LanczosFilter, RESIZE_BLUR
	);
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
static int pixel_to_aacolor(const PixelWand *p)
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
static void aalib_write_wand(
	aa_context *ctx,
	MagickWand *mw
)
{
	// Resize to virtual size
	size_t vwidth = aa_imgwidth(ctx);
	size_t vheight = aa_imgheight(ctx);
	wand_resize(mw, vwidth, vheight);
	
	// Write data to AAlib framebuffer
	PixelIterator *it = NewPixelIterator(mw);
	PixelWand **pixels = NULL;
	size_t n_wands;
	for (size_t y = 0; y < vheight; y++) {
		pixels = PixelGetNextIteratorRow(it, &n_wands);
		for (size_t x = 0; x < vwidth; x++) {
			aa_putpixel(
				ctx,
				x, y,
				pixel_to_aacolor(pixels[x])
			);
		}
	}
	DestroyPixelIterator(it);
	
	aa_render(
		ctx,
		&aa_defrenderparams,
		0, 0,
		vwidth, vheight
	);
}

/* Malloc()s ascii and calloc()s ascii.mat rows. */
static struct ascii *ascii_init(
	size_t width,
	size_t height
)
{
	struct ascii *a = malloc(sizeof(struct ascii));
	
	a->width = width;
	a->height = height;
	
	a->mat = malloc(height * sizeof(char*));
	for (size_t y = 0; y < height; y++) {
		a->mat[y] = calloc(width+1, sizeof(char));
	}
	return a;
}

/* Creates ascii from MagickWand. */
static struct ascii *wand_to_ascii(
	MagickWand *mw,
	size_t width,
	size_t height
)
{
	// Initialize AAlib
	aa_defparams.width = width;
	aa_defparams.height = height;
	aa_context *ctx = aa_autoinit(&aa_defparams);
	
	// Write wand to AAlib framebuffer
	aalib_write_wand(ctx, mw);
	
	// Write AAlib text to buffer
	char *text = (char*)aa_text(ctx);
	struct ascii *a = ascii_init(width, height);
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			a->mat[y][x] = text[y * width + x];
		}
	}
	aa_close(ctx);
	return a;
}

/* Creates ascii from file. */
static struct ascii *image_to_ascii(
	const char *file,
	size_t max_width,
	size_t max_height
)
{
	struct ascii *a = NULL;
	
	// Initialize ImageMagick
	MagickWandGenesis();
	if (IsMagickWandInstantiated() == MagickFalse) {
		fprintf(stderr, "error: couldn't initialize ImageMagick\n");
		return a;
	}
	
	// Load image file
	MagickWand *mw = NewMagickWand();
	if (MagickReadImage(mw, file) == MagickFalse) {
		fprintf(stderr, "error: couldn't load image '%s'\n", file);
	}
	else {
		// Create ascii
		size_t width = MagickGetImageWidth(mw);
		size_t height = MagickGetImageHeight(mw);
		width_rescale(&width);
		resize_max(&width, &height, max_width, max_height);
		a = wand_to_ascii(mw, width, height);
	}
	
	DestroyMagickWand(mw);
	MagickWandTerminus();
	return a;
}

/* Remove whitespace from the right. */
static void ascii_optimize(struct ascii *a)
{
	for (size_t y = 0; y < a->height; y++) {
		for (size_t x = a->width-1; x >= 0; x--) {
			if (a->mat[y][x] != ' ') {
				a->mat[y][x+1] = '\0';
				break;
			}
		}
	}
}

static void ascii_print(struct ascii *a)
{
	for (size_t y = 0; y < a->height; y++) {
		fprintf(stdout, "%s\n", a->mat[y]);
	}
}

static void ascii_destroy(struct ascii *a)
{
	for (size_t y = 0; y < a->height; y++) {
		free(a->mat[y]);
	}
	free(a->mat);
	free(a);
}

int main(
	int argc,
	char *argv[]
)
{
	// Handle command-line arguments
	struct config cfg;
	config_init(&cfg);
	args_parse(&cfg, argc, argv);
	
	// Print help/version/usage
	if (cfg.help_f) {
		fprintf(stdout, "%s\n%s", usage_str, help_str);
		return 0;
	}
	if (cfg.version_f) {
		fprintf(stdout, "%s", version_str);
		return 0;
	}
	if (cfg.err_f) {
		fprintf(stdout, "%s", usage_str);
		return 1;
	}
	
	// Create ascii
	struct ascii *a = image_to_ascii(
		cfg.file,
		cfg.max_width,
		cfg.max_height
	);
	if (a == NULL) {
		return 1;
	}
	
	// Optimize
	if (cfg.optimize_f) {
		ascii_optimize(a);
	}
	
	// Print ascii
	ascii_print(a);
	ascii_destroy(a);
	return 0;
}

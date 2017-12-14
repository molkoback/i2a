/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include "i2a.h"
#include "term.h"
#include "mat.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define VERSION "1.2.1"

static const char *usage_str = "usage: i2a [options] <image>\n";
static const char *help_str = (
	"options:\n"
	"  -h                print this help message\n"
	"  -x <int>          maximum width\n"
	"  -y <int>          maximum height\n"
	"  -t                use the terminal width and height\n"
	"  -m <double>       terminal width multiplier\n"
	"  -i                invert colors\n"
	"  -o                remove whitespace from the right\n"
	"  -I                print info about the generated ascii\n"
	"  -V                print version\n"
);
static const char *version_str = (
	"i2a v"VERSION"\n"
	"Copyright (c) 2017 molko <molkoback@gmail.com>\n"
	"Distributed under WTFPL v2\n"
);

static inline void print_info(struct mat *m)
{
	fprintf(stdout, "\n");
	for (int i = 0; i < m->width; i++) {
		fprintf(stdout, "-");
	}
	fprintf(stdout, "\n");
	fprintf(stdout, "Size: %dx%d\n", (int)m->width, (int)m->height);
	fprintf(stdout, "Char count: %d\n", (int)mat_charcount(m));
}

int main(
	int argc,
	char *argv[]
)
{
	// Init i2a
	struct i2a_context ctx;
	i2a_context_init(&ctx);
	
	int info_f = 0;
	
	// Disable getopt error messages
	extern int opterr;
	opterr = 0;
	
	int c;
	while ((c = getopt(argc, argv, "hVioIx:y:tm:")) != -1) {
		switch(c) {
		case 'h':
			fprintf(stdout, "%s\n%s", usage_str, help_str);
			return 0;
		case 'V':
			fprintf(stdout, "%s", version_str);
			return 0;
		case 'i':
			ctx.cfg.invert_f = 1;
			break;
		case 'o':
			ctx.cfg.optimize_f = 1;
			break;
		case 'I':
			info_f = 1;
			break;
		case 'x':
			if ((ctx.cfg.max_width = atoi(optarg)) == 0) {
				fprintf(stdout, "error: invalid width '%s'\n", optarg);
				return 1;
			}
			break;
		case 'y':
			if ((ctx.cfg.max_height = atoi(optarg)) == 0) {
				fprintf(stdout, "error: invalid height '%s'\n", optarg);
				return 1;
			}
			break;
		case 't':
			get_term_size(&ctx.cfg.max_width, &ctx.cfg.max_height);
			break;
		case 'm':
			if ((ctx.cfg.term_width_mul = atof(optarg)) == 0.0) {
				fprintf(stdout, "error: invalid multiplier '%s'\n", optarg);
				return 1;
			}
			break;
		default:
			fprintf(stdout, "error: invalid option '-%c'\n", optopt);
			return 1;
		}
	}
	if (optind < argc) {
		ctx.cfg.file = argv[optind];
	} else {
		fprintf(stdout, "%s", usage_str);
		return 1;
	}
	
	// Run the main program
	if (i2a_run(&ctx) < 0) {
		return 1;
	}
	
	// Print ascii
	mat_print(ctx.ascii);
	if (info_f) {
		print_info(ctx.ascii);
	}
	mat_destroy(ctx.ascii);
	return 0;
}

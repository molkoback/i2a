/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef I2A_H
#define I2A_H

#include "mat.h"

#include <stddef.h>

#define I2A_VERSION "1.2.2"

enum {
	I2A_OK = 0,
	I2A_ERR_FILE = -1,
	I2A_ERR_IMAGEMAGICK = -2,
	I2A_ERR_AA = -3
};

struct i2a_config {
	int invert_f;
	int optimize_f;
	size_t max_width;
	size_t max_height;
	double term_width_mul;
	char *file;
};

struct i2a_context {
	struct i2a_config cfg;
	struct mat *ascii;
};

void i2a_context_init(struct i2a_context *ctx);

/* Runs i2a and returns an error code. */
int i2a_run(struct i2a_context *ctx);

#endif

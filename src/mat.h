/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef MAT_H
#define MAT_H

#include <stddef.h>

struct mat {
	size_t width;
	size_t height;
	char **data;
};

/* Malloc()s ascii and calloc()s mat.data rows. */
struct mat *mat_init(
	size_t width,
	size_t height
);

void mat_destroy(struct mat *m);

/* Remove whitespace from the right. */
void mat_optimize(struct mat *m);

void mat_print(struct mat *m);

#endif

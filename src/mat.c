/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include "mat.h"

#include <stdlib.h>
#include <stdio.h>

/* Malloc()s ascii and calloc()s mat.data rows. */
struct mat *mat_init(
	size_t width,
	size_t height
)
{
	struct mat *m = malloc(sizeof(struct mat));
	
	m->width = width;
	m->height = height;
	
	m->data = malloc(height * sizeof(char*));
	for (size_t y = 0; y < height; y++) {
		m->data[y] = calloc(width+1, sizeof(char));
	}
	return m;
}

void mat_destroy(struct mat *m)
{
	for (size_t y = 0; y < m->height; y++) {
		free(m->data[y]);
	}
	free(m->data);
	free(m);
} 

/* Remove whitespace from the right. */
void mat_optimize(struct mat *m)
{
	for (size_t y = 0; y < m->height; y++) {
		for (size_t x = m->width-1; x >= 0; x--) {
			if (m->data[y][x] != ' ') {
				m->data[y][x+1] = '\0';
				break;
			}
		}
	}
}

void mat_print(struct mat *m)
{
	for (size_t y = 0; y < m->height; y++) {
		fprintf(stdout, "%s\n", m->data[y]);
	}
}

/* Count non-0 characters. */
size_t mat_charcount(struct mat *m)
{
	size_t count = 0;
	for (size_t y = 0; y < m->height; y++) {
		for (size_t x = 0; x < m->width; x++) {
			if (m->data[y][x] == '\0') {
				break;
			}
			count++;
		}
	}
	return count;
}

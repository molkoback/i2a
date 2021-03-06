/* 
 * Copyright (c) 2017 molko <molkoback@gmail.com>
 * 
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

#include "term.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

void get_term_size(
	size_t *width,
	size_t *height
)
{
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	*width = ws.ws_col;
	*height = ws.ws_row;
}

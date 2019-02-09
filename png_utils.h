#ifndef PNG_UTILS_H__
#define PNG_UTILS_H__
/*
	Copyright (C) 2015 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Gaseous Giganticus.

	Gaseous Giganticus is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Gaseous Giganticus is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Gaseous Giganticus; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <png.h>

int png_utils_write_png_image(const char *filename, unsigned char *pixels, int w, int h, int has_alpha, int invert);

char *png_utils_read_png_image(const char *filename, int flipVertical, int flipHorizontal,
        int pre_multiply_alpha,
        int *w, int *h, int *hasAlpha, char *whynot, int whynotlen);

#endif

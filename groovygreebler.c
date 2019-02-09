/*
	Copyright (C) 2019 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of groovygreebler.

	groovygreebler is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	groovygreebler is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with groovygreebler; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "quat.h"
#include "png_utils.h"

#define DIM 1024

static void calculate_normal(unsigned char *heightmap, union vec3 *normalmap, int i, int j, int dim)
{
	int i1, i2, j1, j2;
	int p1, p2, dzdx[3], dzdy[3];
	union vec3 n;

	i1 = i - 1;
	if (i1 < 0)
	        i1 = i;
	i2 = i + 1;
	if (i2 >= dim)
	        i2 = i;
	j1 = j - 1;
	if (j1 < 0)
	        j1 = j;
	j2 = j + 1;
	if (j2 >= dim)
	        j2 = j;

	/* Average over the surrounding 3x3 pixels in x and in y, emphasizing the central regions
	 * (Sobel filter,  https://en.wikipedia.org/wiki/Sobel_operator )
	 */
	p1 = (j1 * dim + i1);
	p2 = (j1 * dim + i2);
	dzdx[0] = (int) heightmap[p1] - (int) heightmap[p2];
	p1 = (j * dim + i1);
	p2 = (j * dim + i2);
	dzdx[1] = (int) heightmap[p1] - (int) heightmap[p2];
	p1 = (j2 * dim + i1);
	p2 = (j2 * dim + i2);
	dzdx[2] = (int) heightmap[p1] - (int) heightmap[p2];

	p1 = (j1 * dim + i1);
	p2 = (j2 * dim + i1);
	dzdy[0] = (int) heightmap[p2] - (int) heightmap[p1];
	p1 = (j1 * dim + i);
	p2 = (j2 * dim + i);
	dzdy[1] = (int) heightmap[p2] - (int) heightmap[p1];
	p1 = (j1 * dim + i2);
	p2 = (j2 * dim + i2);
	dzdy[2] = (int) heightmap[p2] - (int) heightmap[p1];

	dzdx[0] = dzdx[0] + 2 * dzdx[1] + dzdx[1];
	dzdy[0] = -dzdy[0] - 2 * dzdy[1] - dzdy[1];

	n.v.x = ((float) dzdx[0] / 4.0) / 127.0f + 0.5;
	n.v.y = ((float) dzdy[0] / 4.0) / 127.0f + 0.5;
	n.v.z = 1.0f;
	normalmap[j * dim + i] = n;
}

static void calculate_normalmap(unsigned char *heightmap, union vec3 *normalmap, int dim)
{
	int i, j;

	for (i = 0; i < dim; i++)
		for (j = 0; j < dim; j++)
			calculate_normal(heightmap, normalmap, i, j, dim);
}

static void initialize_heightmap(unsigned char *h, int xdim, int ydim)
{
	memset(h, 128, xdim * ydim);
}

unsigned char heightmap[DIM][DIM];
union vec3 normalmap[DIM][DIM];

static unsigned char *allocate_heightmap(int dim)
{
	return malloc(dim * dim);
}

static union vec3 *allocate_normalmap(int dim)
{
	return malloc(sizeof(union vec3) * dim * dim);
}

static unsigned char *allocate_output_image(int dim)
{
	unsigned char *x = malloc(4 * dim * dim);
	memset(x, 0, 4 * dim * dim);
	return x;
}

static void paint_height_map(unsigned char *image, unsigned char *hmap, int dim, float min, float max)
{
	int f, i, j;
	float r; 
	unsigned char c;
	int p, h;

	for (f = 0; f < 6; f++) {
		for (i = 0; i < dim; i++) {
			for (j = 0; j < dim; j++) {
				h = (j * dim + i);
				p = h * 4;
				r = hmap[h];
				r = (r - min) / (max - min);
				c = (unsigned char) (r * 255.0f);
				image[p + 0] = c;
				image[p + 1] = c;
				image[p + 2] = c;
				image[p + 3] = 255;
			}
		}
	}
}

static void write_image(const char *filename, unsigned char *img, int dim)
{
	int rc;

	rc = png_utils_write_png_image(filename, img, dim, dim, 1, 0);
	if (rc)
		fprintf(stderr, "Failed to write file %s: %s\n", filename, strerror(errno));
}

int main(int argc, char *argv[])
{
	unsigned char *heightmap, *hmap_img;
	union vec3 *normalmap;

	heightmap = allocate_heightmap(DIM);
	normalmap = allocate_normalmap(DIM);
	hmap_img = allocate_output_image(DIM);

	initialize_heightmap(heightmap, DIM, DIM);
	calculate_normalmap(heightmap, normalmap, DIM);

	paint_height_map(hmap_img, heightmap, DIM, 0, 255);

	write_image("heightmap.png", hmap_img, DIM);

	free(hmap_img);
	free(normalmap);
	free(heightmap);
	return 0;
}

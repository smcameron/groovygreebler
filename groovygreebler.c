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
#include <sys/time.h>
#include <math.h>

#include "quat.h"
#include "png_utils.h"
#include "bline.h"

#define DIM 1024

#define LINE 0
#define RECTANGLE 1
#define CIRCLE 2
#define ANNULUS_SECTOR 3

const int xo[] = { 1, 0 };
const int yo[] = { 0, 1 };

int max(int a, int b)
{
	return (a > b ? a : b);
}

int min(int a, int b)
{
	return (a < b ? a : b);
}

struct primitive {
	union params {
		struct {
			int r;
		} circle;
		struct {
			int w, h;
		} rectangle;
		struct {
			int len, dir;
		} line;
		struct {
			int inner_r, outer_r;
			float a1, a2;
		} annulus_sector;
	} p;
	int type;
	int in_or_out;
	int x, y;
};

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

static void paint_normal_map(unsigned char *normal_image, union vec3 *normal_map, int dim)
{
	int i, j;
	char red, green, blue;
	int p;

	for (i = 0; i < dim; i++) {
		for (j = 0; j < dim; j++) {
			p = (j * dim + i);
			red = normal_map[p].v.x * 255;
			green = normal_map[p].v.y * 255;
			blue = normal_map[p].v.z * 255;
			normal_image[4 * p + 0] = red;
			normal_image[4 * p + 1] = green;
			normal_image[4 * p + 2] = blue;
			normal_image[4 * p + 3] = 255;
		}
	}
}

static void paint_height_map(unsigned char *image, unsigned char *hmap, int dim, float min, float max)
{
	int i, j;
	float r;
	unsigned char c;
	int p, h;

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

static void write_image(const char *filename, unsigned char *img, int dim)
{
	int rc;

	rc = png_utils_write_png_image(filename, img, dim, dim, 1, 0);
	if (rc)
		fprintf(stderr, "Failed to write file %s: %s\n", filename, strerror(errno));
}

static void set_height(unsigned char *heightmap, int x, int y, int h, int dim)
{
	int p, new_height;

	if (x < 0 || x >= dim)
		return;
	if (y < 0 || y >= dim)
		return;
	p = y * dim + x;
	new_height = (int) heightmap[p] + h;
	if (new_height < 0)
		new_height = 0;
	else if (new_height > 255)
		new_height = 255;
	heightmap[p] = new_height;
}

static void add_groove(unsigned char *heightmap, int dim, int x, int y, int len, int dir, int in_or_out)
{
	int i;

	x -= (len / 2) * xo[dir];
	y -= (len / 2) * yo[dir];
	for (i = 0; i < len; i++) {
		set_height(heightmap, x, y, in_or_out * 30, dim);
		set_height(heightmap, x + yo[dir], y + xo[dir], in_or_out * 15, dim);
		set_height(heightmap, x - yo[dir], y - xo[dir], in_or_out * 15, dim);
		x += xo[dir];
		y += yo[dir];
	}
}

static void add_random_groove(unsigned char *heightmap, int dim)
{
	int len, x, y, dir;
	int in_or_out = 2 * (rand() % 2) - 1;

	dir = rand() % 2;
	x = rand() % dim;
	y = rand() % dim;
	len = rand() % (dim / 2);

	add_groove(heightmap, dim, x, y, len, dir, in_or_out);
}

static void add_random_grooves(unsigned char *heightmap, int dim, int count)
{
	int i;

	for (i = 0; i < count; i++)
		add_random_groove(heightmap, dim);
}

static void greeble_area(unsigned char *heightmap, int dim, int x1, int y1, int x2, int y2, int limit);
static void add_rectangle(unsigned char *heightmap, int dim, int x, int y, int width, int height, int in_or_out)
{
	int i, j;
	int lox, hix, loy, hiy;

	lox = x - width / 2;
	hix = x + width / 2;
	loy = y - height / 2;
	hiy = y + height / 2;

	if ((rand() % 5) == 0) {
		greeble_area(heightmap, dim, lox, loy, hix, hiy, 32);
		return;
	}

	for (i = lox + 1; i < hix - 1; i++) {
		for (j = loy + 1; j < hiy - 1; j++) {
			set_height(heightmap, i, j, in_or_out * 30, dim);
		}
	}

	for (i = lox; i < hix; i++) {
		set_height(heightmap, i, loy, in_or_out * 15, dim);
		set_height(heightmap, i, hiy, in_or_out * 15, dim);
	}
	for (i = loy; i < hiy; i++) {
		set_height(heightmap, lox, i, in_or_out * 15, dim);
		set_height(heightmap, hix, i, in_or_out * 15, dim);
	}
}

static void add_random_rectangle(unsigned char *heightmap, int dim)
{
	int x, y, width, height;
	int in_or_out = 2 * (rand() % 2) - 1;

	x = rand() % dim;
	y = rand() % dim;
	width = rand() % 50 + 20;
	height = rand() % 50 + 20;

	add_rectangle(heightmap, dim, x, y, width, height, in_or_out);
}

static void add_random_rectangles(unsigned char *heightmap, int dim, int count)
{
	int i;

	for (i = 0; i < count; i++)
		add_random_rectangle(heightmap, dim);
}

static void add_circle(unsigned char *heightmap, int dim, int x, int y, int radius, int in_or_out)
{
	int i, j;
	int lox, hix, loy, hiy;
	float d, dx, dy;

	lox = x - radius;
	hix = x + radius;
	loy = y - radius;
	hiy = y + radius;

	for (i = lox + 1; i < hix - 1; i++) {
		dx = x - i;
		for (j = loy + 1; j < hiy - 1; j++) {
			dy = y - j;
			d = dy * dy + dx * dx;
			if (d < radius * radius)
				set_height(heightmap, i, j, in_or_out * 20, dim);
		}
	}
}

struct bline_context {
	unsigned char *heightmap;
	int dim;
	int in_or_out;
};

static void plot_point(int x, int y, void *context)
{
	struct bline_context *c = context;

	set_height(c->heightmap, x, y, c->in_or_out * 20, c->dim);
}

static void add_annulus_sector(unsigned char *heightmap, int dim, int x, int y,
				float a1, float a2, int r1, int r2, int in_or_out, int limit)
{
	struct bline_context c;
	int x1, y1, x2, y2, x3, y3, x4, y4;

	c.heightmap = heightmap;
	c.dim = dim;
	c.in_or_out = in_or_out;

	x1 = x + cos(a1) * r1;
	y1 = y - sin(a1) * r1;
	x2 = x + cos(a1) * r2;
	y2 = y - sin(a1) * r2;
	x3 = x + cos(a2) * r1;
	y3 = y - sin(a2) * r1;
	x4 = x + cos(a2) * r2;
	y4 = y - sin(a2) * r2;

	bline(x1, y1, x2, y2, plot_point, &c);
	bline(x2, y2, x4, y4, plot_point, &c);
	bline(x4, y4, x3, y3, plot_point, &c);
	bline(x3, y3, x1, y1, plot_point, &c);
}

static void add_random_circle(unsigned char *heightmap, int dim)
{
	int x, y, radius;
	int in_or_out = 2 * (rand() % 2) - 1;

	x = rand() % dim;
	y = rand() % dim;
	radius = rand() % 50 + 20;

	add_circle(heightmap, dim, x, y, radius, in_or_out);
}

static void add_random_circles(unsigned char *heightmap, int dim, int count)
{
	int i;

	for (i = 0; i < count; i++)
		add_random_circle(heightmap, dim);
}

static void subdivide_circle(unsigned char *heightmap, int dim, int x, int y, int r, int in_or_out, int limit)
{
	float ainc, a2, a1 = 0;
	int r1, r2;

	r1 = r * (((float) (rand() % 50) + 30.0) / 100.0);
	r2 = r;
	a2 = a1;
	do {
		ainc = 2.0 * M_PI / ((rand() % 10) + 10);
		a2 = a2 + ainc;
		if (a2 > 2.0 * M_PI) {
			a2 = 2.0 * M_PI;
			break;
		}
		add_annulus_sector(heightmap, dim, x, y, a1, a2, r1, r2, in_or_out, limit);
		a1 = a2;
	} while (a1 < 2.0 * M_PI);
	if (r1 * 2 > limit)
		subdivide_circle(heightmap, dim, x, y, r1, in_or_out, limit);
}

static void add_primitive(unsigned char *heightmap, int dim, struct primitive *p, int limit)
{
	switch (p->type) {
	case LINE:
		add_groove(heightmap, dim, p->x, p->y, p->p.line.len, p->p.line.dir, p->in_or_out);
		break;
	case RECTANGLE:
		add_rectangle(heightmap, dim, p->x, p->y, p->p.rectangle.w, p->p.rectangle.h, p->in_or_out);
		break;
	case CIRCLE:
		add_circle(heightmap, dim, p->x, p->y, p->p.circle.r, p->in_or_out);
		if (p->p.circle.r * 2 > limit)
			subdivide_circle(heightmap, dim, p->x, p->y, p->p.circle.r, p->in_or_out, limit);
		break;
	case ANNULUS_SECTOR:
		add_annulus_sector(heightmap, dim, p->x, p->y,
			p->p.annulus_sector.a1, p->p.annulus_sector.a2,
			p->p.annulus_sector.inner_r, p->p.annulus_sector.outer_r, p->in_or_out, limit);
		break;
	default:
		break;
	}
}

static void add_row_of_primitives(unsigned char *heightmap, int dim, int dir, int count, int inc, struct primitive *p, int limit)
{
	int i;

	for (i = 0; i < count; i++) {
		add_primitive(heightmap, dim, p, limit);
		p->x += xo[dir] * inc;
		p->y += yo[dir] * inc;
	}
}

static void add_random_row_of_random_primitives(unsigned char *heightmap, int dim, int limit)
{
	int dir, inc, count;
	struct primitive p;

	count = rand() % 7 + 3;
	dir = rand() % 2;
	p.type = rand() % 3;
	p.x = rand() % dim;
	p.y = rand() % dim;
	p.in_or_out = 2 * (rand() % 2) - 1;

	switch (p.type) {
	case CIRCLE:
		p.p.circle.r = rand() % 35 + 5;
		inc = p.p.circle.r * 2.3;
		break;
	case RECTANGLE:
		p.p.rectangle.w = rand() % 35 + 5;
		p.p.rectangle.h = rand() % 35 + 5;
		inc = 1.2 * max(p.p.rectangle.w, p.p.rectangle.h);
		break;
	case LINE:
		p.p.line.len = rand() % (dim / 2);
		p.p.line.dir = !dir;
		inc = 5;
		break;
	default:
		break;
	}
	add_row_of_primitives(heightmap, dim, dir, count, inc, &p, limit);
}

static void add_random_rows(unsigned char *heightmap, int dim, int count, int limit)
{
	int i;

	for (i = 0; i < count; i++)
		add_random_row_of_random_primitives(heightmap, dim, limit);
}

static void populate_rects(unsigned char *heightmap, int dim, int x1, int y1, int x2, int y2,
				__attribute__((unused)) int limit)
{
	int dx, dy;
	int count, dir, incx, incy;
	struct primitive p;

	dx = abs(x2 - x1) - 10;
	dy = abs(y2 - y1) - 10;
	dir = rand() % 2;
	count = rand() % 10;
	if (!count)
		return;
	incx = (xo[dir] * dx) / count;
	incy = (yo[dir] * dy) / count;

	p.in_or_out = 2 * (rand() % 2) - 1;
	p.type = RECTANGLE;
	p.x = x1 + dx * yo[dir] / 2 + incx * xo[dir] / 2 + 5;
	p.y = y1 + dy * xo[dir] / 2 + incy * yo[dir] / 2 + 5;
	p.p.rectangle.w = incx + yo[dir] * dx;
	p.p.rectangle.h = incy + xo[dir] * dy;
	add_row_of_primitives(heightmap, dim, dir, count, incx + incy, &p, limit);
}

static void populate_circles(unsigned char *heightmap, int dim, int x1, int y1, int x2, int y2, int limit)
{
	int dx, dy;
	int count, dir, incx, incy, r;
	int remainder;
	struct primitive p;

	dx = abs(x2 - x1);
	dy = abs(y2 - y1);
	if (dx < limit || dy < limit)
		return;
	dir = rand() % 2;

	if (dx > dy) {
		count = dx / dy;
		remainder = dx % dy;
		dir = 0;
		r = 0.45 * dy;
	} else {
		count = dy / dx;
		remainder = dy % dx;
		dir = 1;
		r = 0.45 * dx;
	}
	incx = (xo[dir] * dy);
	incy = (yo[dir] * dx);

	p.in_or_out = 2 * (rand() % 2) - 1;
	p.type = CIRCLE;
	p.x = x1 + dx * yo[dir] / 2 + incx * xo[dir] / 2;
	p.y = y1 + dy * xo[dir] / 2 + incy * yo[dir] / 2;
	p.p.circle.r = r;
	add_row_of_primitives(heightmap, dim, dir, count, incx + incy, &p, limit);
	if (remainder > limit) {
		x1 = x1 + incx * count * xo[dir];
		y1 = y1 + incy * count * yo[dir];
		greeble_area(heightmap, dim, x1, y1, x2, y2, limit);
	}
}

static void populate_greebles(unsigned char *heightmap, int dim, int x1, int y1, int x2, int y2, int limit)
{
	int c;

	c = 0;

	c = rand() % 4;

	switch (c) {
	case 0:
	case 1:
	case 2:
		populate_rects(heightmap, dim, x1, y1, x2, y2, limit);
		break;
	case 3:
		populate_circles(heightmap, dim, x1, y1, x2, y2, limit);
		break;
	default:
		break;
	}
}

static void greeble_area(unsigned char *heightmap, int dim, int x1, int y1, int x2, int y2, int limit)
{
	int dx, dy, x, y, dir;

	dx = abs(x2 - x1);
	dy = abs(y2 - y1);
	if (dx > dy) {
		if (dx < limit || (dx < limit * 8 && (rand() % 5) == 0)) {
			populate_greebles(heightmap, dim, x1, y1, x2, y2, limit);
			return;
		}
		x = min(x1, x2);
		x += dx / 2;
		x += rand() % (dx / 2) - (dx / 4);
		y = min(y1, y2);
		y += dy / 2;
		dir = 1;
		add_groove(heightmap, dim, x, y, dy, dir, 1);
		greeble_area(heightmap, dim, x1, y1, x, y2, limit);
		greeble_area(heightmap, dim, x, y1, x2, y2, limit);
	} else {
		if (dy < limit || (dy < limit * 8 && (rand() % 5) == 0)) {
			populate_greebles(heightmap, dim, x1, y1, x2, y2, limit);
			return;
		}
		x = min(x1, x2);
		x += dx / 2;
		y = min(y1, y2);
		y += dy / 2;
		y += rand() % (dy / 2) - (dy / 4);
		dir = 0;
		add_groove(heightmap, dim, x, y, dx, dir, 1);
		greeble_area(heightmap, dim, x1, y1, x2, y, limit);
		greeble_area(heightmap, dim, x1, y, x2, y2, limit);
	}
}

int main(int argc, char *argv[])
{
	unsigned char *heightmap, *hmap_img, *normal_img;
	union vec3 *normalmap;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	heightmap = allocate_heightmap(DIM);
	normalmap = allocate_normalmap(DIM);
	hmap_img = allocate_output_image(DIM);
	normal_img = allocate_output_image(DIM);

	initialize_heightmap(heightmap, DIM, DIM);

	//add_random_grooves(heightmap, DIM, 100);
	// add_random_rectangles(heightmap, DIM, 20);
	// add_random_circles(heightmap, DIM, 0);
	//add_random_rows(heightmap, DIM, 150, 32);

	greeble_area(heightmap, DIM, 0, 0, DIM - 1 , DIM - 1, 32);

	calculate_normalmap(heightmap, normalmap, DIM);

	paint_height_map(hmap_img, heightmap, DIM, 0, 255);
	paint_normal_map(normal_img, normalmap, DIM);

	write_image("heightmap.png", hmap_img, DIM);
	write_image("normalmap.png", normal_img, DIM);

	free(normal_img);
	free(hmap_img);
	free(normalmap);
	free(heightmap);
	return 0;
}

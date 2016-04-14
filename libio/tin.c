/*
 * Copyright 2004 Bill Cheswick <ches@cheswick.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Cheswick.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	tin.c - test video input...no video device needed.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "libutil/util.h"
#include "io.h"

#define SAMPLE_FILE	"sample.jpeg"

static image buf_in;
image *video_in;

static int hue = 0;
static int saturation = 0;
static int brightness = 0;
static int contrast = 0;

void
set_hue(char h) {
	hue = h;
}

void
set_saturation(u_char s) {
fprintf(stderr, "set_sat: %d\n", s);
	saturation = s;
}

void
set_brightness(char b) {
	brightness = b;
}

void
set_contrast(u_char c) {
	contrast = c;
}

char
get_hue(void) {
	return hue;
}

u_char
get_saturation() {
	return saturation;
}

char
get_brightness(void) {
	return brightness;
}

u_char
get_contrast(void) {
	return contrast;
}

void
gen_test_image(void) {
	int x, y;

	for (y=0; y<MAX_Y; y++)
		for (x=0; x<MAX_X; x++) {
			Pixel p = LightGreen;

			switch (y / (MAX_Y/4)) {
			case 0:
				p.r = p.g = p.b = 0;
				p.r = (Z*x)/(MAX_X);
				break;
			case 1:
				p.r = p.g = p.b = 0;
				p.g = (Z*x)/(MAX_X);
				break;
			case 2:
				p.r = p.g = p.b = 0;
				p.b = (Z*x)/(MAX_X);
				break;
			case 3:
				switch (x/(MAX_X/3)) {
				case 0:	p = SETRGB(Z*y/(MAX_Y/4),0,0);	break;
				case 1:	p = SETRGB(0, Z*y/(MAX_Y/4),0);	break;
				case 2: p = SETRGB(0, 0, Z*y/(MAX_Y/4)); break;
				};
				break;
			}
			buf_in[y][x] = p;
		}
	for (y=0; y<MAX_Y; y += MAX_Y/10)
		for (x=0; x<MAX_X/2; x++)
			buf_in[y][x] = (x & 0x1) ? Black : White;
}

void
init_video_in(void) {
	char *path = find_file(SAMPLE_FILE, "lib");
	if (path && read_jpeg_image(path, &buf_in, 1))
		return;
	gen_test_image();
}

void
end_video_in(void) {
}

/*
 * We have a static image.  Move it around so people can see what is really
 * going on.
 */

static image current;

Rectangle crop = {{20, 20}, {MAX_X-20, MAX_Y-20}};
static Point translate = {0,0};
Point v = {1, -1};

static l_time_t last_time = 0;

#define IMAGE_MOVE	60	/* ms */

void
grab_video_in(void) {
	int x,y;
	int new_tx, new_ty;
	l_time_t now = rtime();

	if (last_time == 0 || last_time + IMAGE_MOVE < now) {
		new_tx = translate.x + v.x;
		new_ty = translate.y + v.y;
		if (new_tx >= -crop.min.x && new_tx + crop.max.x <= MAX_X)
			translate.x = new_tx;
		else
			v.x = -v.x;
		if (new_ty >= -crop.min.y && new_ty + crop.max.y <= MAX_Y)
			translate.y = new_ty;
		else
			v.y = -v.y;
		if (random()%50 == 0)
			v.x = -v.x;
		if (random()%50 == 0)
			v.y = -v.y;
		last_time = now;
	}

	for (y=0; y<MAX_Y; y++) {
		int ty = crop.min.y+translate.y;
		if (y < ty || y >= crop.max.y+translate.y) {
			for (x=0; x<MAX_X; x++)
				current[y][x] = White;
		} else {
			int tx = crop.min.x+translate.x;
			for (x=0; x<tx; x++)
				current[y][x] = White;
			for (; x<tx + DX(crop); x++)
				current[y][x] = buf_in[y-ty][x-tx];
			for (; x<MAX_X; x++)
				current[y][x] = White;
		}
	}
	video_in = &current;
}

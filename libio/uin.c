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
 *	uin.c - Dlink USB camera video input.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <ppm.h>
#include <pnm.h>

#include "libutil/util.h"
#include "io.h"
#include "ov.h"
#include "ov511.h"

char *device = 0;

static image buf_in;

image *video_in = &buf_in;

/* stubs */
void
set_brightness(char brightness) {
}

void
set_contrast(u_char contrast) {
}

void
set_hue(char hue) {
}

void
set_saturation(u_char sat) {
}

char
get_brightness(void) {
	return 0;
}

u_char
get_contrast(void) {
	return 0;
}

char
get_hue(void) {
	return 0;
}

u_char
get_saturation() {
	return 0;
}

void
init_video_in(void) {
	setup_ov511(1, 0);	/* 320x240 size, you figure out the device name */
}

void
end_video_in(void) {
	finish_ov511();
}

void
grab_video_in(void) {
	int x, y;
	xel **pixels;
	pixel *pi;

	ov511_new_frame();
	pixels = grab_ov511();
	pi = *pixels;
	for (y=MAX_Y-1; y>= 0; y--) {
		Pixel *po = (Pixel *)&buf_in[y][0];
		for (x=MAX_X-1; x>=0; x--, pi++) {
			Pixel p;
			p.r = PPM_GETR(*pi);
			p.g = PPM_GETG(*pi);
			p.b = PPM_GETB(*pi);
			p.a = Z;
			po[x] = p;
		}
	}

}

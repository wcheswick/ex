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
 *	hin.c - High speed BT484 video input.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

#include <dev/bktr/ioctl_meteor.h>
#include <dev/bktr/ioctl_bt848.h>

#include "libutil/util.h"
#include "io.h"
#include "ov.h"

#define	HMAX_X	640
#define HMAX_Y	480

typedef struct Pixelmeteor	Pixelmeteor;
struct Pixelmeteor {
	u_char b,g,r,a;
};

typedef Pixelmeteor meteorbuf[HMAX_Y][HMAX_X];

static image vinb;

image *video_in = &vinb;

/*
 * The Brooktree has a 16-bit mode that cuts the number of bytes in
 * half and probably runs faster.  But there are endian issues and we
 * have to touch every pixel anyway to reverse the horizontal (so the camera is
 * a mirror and the vertical, because our origin in in the lower left.  So
 * RGB24 (which is actually RGB 32 is just easier.
 */

struct meteor_geomet geo;
u_char *mmbuf;
static int fd;

#define	BKDEV	"/dev/bktr"

/*
 * There are two mysteries about the meteor brightness settings:
 *	1) Why is the brightness value a signed character, rather
 *		than 0-255 range given on the data sheet?  Kernel bug?
 *	2) Why do I have to set, get, and set the brightness?
 *
 * These mysteries are probably related, but I don't care.  This works,
 * and I have bigger fish to fry.  Though I don't like fish.
 */

void
set_hue(char h) {
	if (ioctl(fd, METEORSHUE, &h) < 0)
		perror("METEORSHUE");
	if (ioctl(fd, METEORGHUE, &h) < 0)
		perror("METEORGHUE");
	if (ioctl(fd, METEORSHUE, &h) < 0)
		perror("METEORSHUE");
}

void
set_saturation(u_char s) {
	if (ioctl(fd, METEORSCSAT, &s) < 0)
		perror("METEORSCSAT");
	if (ioctl(fd, METEORGCSAT, &s) < 0)
		perror("METEORGCSAT");
	if (ioctl(fd, METEORSCSAT, &s) < 0)
		perror("METEORSCSAT");
}

void
set_brightness(char b) {
	if (ioctl(fd, METEORSBRIG, &b) < 0)
		perror("METEORSBRIG");
	if (ioctl(fd, METEORGBRIG, &b) < 0)
		perror("METEORGBRIG");
	if (ioctl(fd, METEORSBRIG, &b) < 0)
		perror("METEORSBRIG");
}

void
set_contrast(u_char c) {
	if (ioctl(fd, METEORSCONT, &c) < 0)
		perror("METEORSCONT");
	if (ioctl(fd, METEORGCONT, &c) < 0)
		perror("METEORGCONT");
	if (ioctl(fd, METEORSCONT, &c) < 0)
		perror("METEORSCONT");
}

char
get_brightness(void) {
	char bright;

	if (ioctl(fd, METEORGBRIG, &bright) < 0)
		perror("METEORGBRIG");
	return bright;
}

u_char
get_contrast(void) {
	u_char contrast;

	if (ioctl(fd, METEORGCONT, &contrast) < 0)
		perror("METEORGCONT");
	return contrast;
}

char
get_hue(void) {
	char hue;

	if (ioctl(fd, METEORGHUE, &hue) < 0)
		perror("METEORGHUE");
	return hue;
}

u_char
get_saturation() {
	u_char sat;

	if (ioctl(fd, METEORGCSAT, &sat) < 0)
		perror("METEORGCSAT");
	return sat;
}

void
init_meteor(void) {
	char *input_dev = getenv("METEOR");
	int c;

	if ((fd = open(BKDEV, O_RDONLY)) < 0) {
		printf("%s: open failed: (%d) %s: %s\n",
			prog, errno, strerror(errno), BKDEV);
		exit(1);
	}

	geo.rows = HMAX_Y;
	geo.columns = HMAX_X;
	geo.frames = 2;
	geo.oformat = METEOR_GEO_RGB24;

	if (ioctl(fd, METEORSETGEO, &geo) < 0) {
		perror("METEORSETGEO");
		exit(1);
	}

	c = METEOR_FMT_NTSC;
	if (ioctl(fd, METEORSFMT, &c) < 0) {
		perror("METEORSFMT");
		exit(1);
	}

	if (input_dev && strcmp(input_dev, "RCA") == 0)
		c = METEOR_INPUT_DEV_RCA;
	else if (input_dev && strcmp(input_dev, "0") == 0)
		c = METEOR_INPUT_DEV0;
	else if (input_dev && strcmp(input_dev, "1") == 0)
		c = METEOR_INPUT_DEV1;
	else if (input_dev && strcmp(input_dev, "2") == 0)
		c = METEOR_INPUT_DEV2;
	else if (input_dev && strcmp(input_dev, "3") == 0)
		c = METEOR_INPUT_DEV3;
	else if (input_dev && strcmp(input_dev, "RGB") == 0)
		c = METEOR_INPUT_DEV_RGB;
	else if (input_dev && strcmp(input_dev, "SVIDEO") == 0)
		c = METEOR_INPUT_DEV_SVIDEO;
	else
		c = METEOR_INPUT_DEV_RCA;

	if (ioctl(fd, METEORSINPUT, &c) < 0) {
		perror("METEORSINPUT");
		exit(1);
	}

	mmbuf=(u_char *)mmap((caddr_t)0, sizeof(meteorbuf), PROT_READ,
		MAP_SHARED, fd, (off_t)0);
	if ((caddr_t)mmbuf == (caddr_t) MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	/* nighttime settings, purely empirical */
	//set_brightness(20);
	//set_contrast(206);
	/* daytime settings */
	//set_brightness(30);
	//set_contrast(187);
	// set_chroma_u(138);
	// set_chroma_v(172);
	set_hue(0);
	set_saturation(117);
	set_brightness(-78);
	set_contrast(122);
}

void
grab_frame(void) {
	int c = METEOR_CAP_SINGLE ;
	if (ioctl(fd, METEORCAPTUR, &c) < 0) {
		perror("METEORCAPTUR 0");
		exit(1);
	}
}

void
start_capture(void) {
	int c = METEOR_CAP_CONTINOUS ;
	if (ioctl(fd, METEORCAPTUR, &c) < 0) {
		perror("METEORCAPTUR 1");
		exit(1);
	}
}

void
stop_capture(void) {
	int c = METEOR_CAP_STOP_CONT ;
	if (ioctl(fd, METEORCAPTUR, &c) < 0) {
		perror("METEORCAPTUR 2");
		exit(1);
	}
	close(fd);
}


void
init_video_in(void) {
	init_meteor();
	start_capture();
}

void
end_video_in(void) {
	stop_capture();
}

/*
 * We grab 640x480, but since we currently only use 320x240, we pick
 * the center of the picture as a cheap zoom.
 */
void
grab_video_in(void) {
	int x, y;
	int minx = (HMAX_X-MAX_X)/2;
	int miny = (HMAX_Y-MAX_Y)/2;
	meteorbuf *buf = (meteorbuf *)mmbuf;
	Pixel *op = &vinb[MAX_Y-1][MAX_X-1];

	for (y=miny; y<miny + MAX_Y; y++) {
		Pixelmeteor *pm = &(*buf)[y][minx];
		for (x=minx; x<minx + MAX_X; x++, pm++) {
			Pixel p;
			p.r = pm->r;
			p.g = pm->g;
			p.b = pm->b;
			p.a = Z;
			*op-- = p;
		}
	}
}

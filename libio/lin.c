/*
 * Copyright 2011 Bill Cheswick <ches@cheswick.com>
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
 *	lin.c - Linux v4l2 /dev/video input driver
 *
 *	V4L2 spec from http://v4l2spec.bytesex.org/spec/r13105.htm
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include <linux/videodev2.h>

#include "libutil/util.h"
#include "io.h"

// The video frame size, perhaps not the desired processing size:

size_t video_frame_size;

image input_frame;
image *video_in = &input_frame;

char vfd = -1;
struct v4l2_capability vcap;

#define NBUFFERS	30

struct buffer {
	void	*addr;
	size_t	length;
};

struct v4l2_buffer *bufinfo;
struct buffer *buffers;
int n_buffers;

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
//V4L2_CID_BRIGHTNESS	integer	Picture brightness, or more precisely, the black level.
	return 0;
}

u_char
get_contrast(void) {
//V4L2_CID_CONTRAST	integer	Picture contrast or luma gain.
	return 0;
}

char
get_hue(void) {
//V4L2_CID_HUE	integer	Hue or color balance.
	return 0;
}

u_char
get_saturation() {
//V4L2_CID_SATURATION	integer	Picture color saturation or chroma gain.
	return 0;
}

int
try_video(int v) {
	struct v4l2_format fmt;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_requestbuffers reqbufs;

	int best_fmt = -1;
	int i;
	char name[100];
	int fd;
	char *cp;


	snprintf(name, sizeof(name), "/dev/video%d", v);
	fprintf(stderr, "%s  ", name);

	fd = open(name, O_RDWR  /*|O_NONBLOCK*/  );
	if (fd < 0) {
		perror(name);
		return 0;	// not found or not openable
	}

	if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0) {
		perror(name);
		return 0;
	}
	fprintf(stderr, "%s:	driver %s\n", name, vcap.driver);
	fprintf(stderr, "%s:	device %s\n", name, vcap.card);
	fprintf(stderr, "%s:	capabilities %08x\n", name, vcap.capabilities);

	if (!(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		return 0;	// capture not available

	if (!(vcap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "streaming not supported\n");
		return 0;	// streaming not available
	}

	// enumerate available inputs:
	fprintf(stderr, "Inputs:\n");
	for (i=0; ; i++) {
		struct v4l2_input input;
		memset(&input, 0, sizeof(input));
		input.index = i;
		if (ioctl(fd, VIDIOC_ENUMINPUT, &input) < 0) {
			if (errno == EINVAL)
				break;
			perror("VIDIOC_ENUMINPUT");
			continue;
		}
		fprintf(stderr, "	%d: %s\n", i, input.name);
		fprintf(stderr, "	    %s\n",
			input.type == V4L2_INPUT_TYPE_TUNER ? "tuner" : "camera");
//		fprintf(stderr, "	    supports %d\n", input.std);
		fprintf(stderr, "	    status   %08x\n", input.status);
	}

	// video standards, skipped

	// user controls may be enumerated, skipped.

	// extended controls (mostly MPEG stuff), skipped.

	// enumerate available formats:

	fprintf(stderr, "Formats:\n");
	for (i=0; ; i++) {
		memset(&fmtdesc, 0, sizeof(fmtdesc));
		fmtdesc.index = i;
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
			if (errno == EINVAL)
				break;
			perror("VIDIOC_ENUM_FMT");
			continue;
		}

//		fprintf(stderr, "	%d: %s\n", i, fmtdesc.description);
//		fprintf(stderr, "	    flags: %08x\n", fmtdesc.flags);
		cp = (char *)&fmtdesc.pixelformat;
//		fprintf(stderr, "	    fmt: %c%c%c%c\n",
//			cp[0], cp[1], cp[2], cp[3]);

// we need YUYV, /* 16  YUV 4:2:2     */
// or maybe UYVY
		// pretty weak criteria: compressed means MPEG or MJPEG

		if (!(fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED) &&
		    best_fmt < 0)
			best_fmt = i;
	}

	if (best_fmt < 0) {
		fprintf(stderr, "no camera format found, aborting\n");
		exit(8);
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		perror("VIDIOC_G_FMT 0");

	cp = (char *)&fmt.fmt.pix.pixelformat;
	fprintf(stderr, "%d x %d	%c%c%c%c\n",
		fmt.fmt.pix.width, fmt.fmt.pix.height, cp[0], cp[1], cp[2], cp[3]);

	fmt.fmt.pix.width = MAX_X;
	fmt.fmt.pix.height = MAX_Y;
//	cp[0] = 'R'; cp[1] = 'G'; cp[2] = 'B'; cp[3] = '4';
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
		perror("VIDIOC_S_FMT");

	cp = (char *)&fmt.fmt.pix.pixelformat;
	fprintf(stderr, "%d x %d	%c%c%c%c\n",
		fmt.fmt.pix.width, fmt.fmt.pix.height, cp[0], cp[1], cp[2], cp[3]);

	fmt.fmt.pix.width = IN_W;
	fmt.fmt.pix.height = IN_H;
//	cp[0] = 'R'; cp[1] = 'G'; cp[2] = 'B'; cp[3] = '4';
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
		perror("VIDIOC_S_FMT");

	cp = (char *)&fmt.fmt.pix.pixelformat;
	fprintf(stderr, "%d x %d	%c%c%c%c\n",
		fmt.fmt.pix.width, fmt.fmt.pix.height, cp[0], cp[1], cp[2], cp[3]);


	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbufs.count = NBUFFERS;
	reqbufs.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
		perror("VIDIOC_REQBUFS");
		fprintf(stderr, "memory-mapped bufinfo not available, aborting\n");
		exit(12);
	}
	n_buffers = reqbufs.count;
	fprintf(stderr, "using %d memory-mapped bufinfo\n", n_buffers);

	bufinfo = (struct v4l2_buffer *)malloc(n_buffers * sizeof(struct v4l2_buffer));
	assert(bufinfo);
	buffers = (struct buffer *)malloc(n_buffers * sizeof(struct buffer));
	assert(buffers);

	for (i=0; i<n_buffers; i++) {
		memset(&bufinfo[i], 0, sizeof(struct v4l2_buffer));
		bufinfo[i].index = i;
		bufinfo[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_QUERYBUF, &bufinfo[i]) < 0) {
			perror("VIDIOC_QUERYBUF");
			exit(14);
		}

		buffers[i].length = bufinfo[i].length;
		buffers[i].addr = mmap (NULL, bufinfo[i].length,
                                 PROT_READ | PROT_WRITE, /* recommended */
                                 MAP_SHARED,             /* recommended */
                                 fd, bufinfo[i].m.offset);

		if (MAP_FAILED == buffers[i].addr) {
			/* If you do not exit here you should unmap() and free()
				the buffers mapped so far. */
			perror ("mmap");
			exit (8);
		}
	}
	return fd;
}

void
init_video_in(void) {
	int i;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (MAX_Y > IN_H || MAX_X > IN_W) {
		fprintf(stderr, "can not grab enough pixels, change video parameters\n");
		exit(15);
	}

	for (i=0; vfd < 0 && i<15; i++)	// /dev/video{0-15}
		vfd = try_video(i);
	if (vfd < 0) {
		fprintf(stderr, "L4V2 could not find usable video device\n");
		exit(1);
	}

	// Enqueue buffers and turn streaming on

	for (i=0; i<n_buffers; i++) {
		bufinfo[i].index = i;
		if (ioctl(vfd, VIDIOC_QBUF, &bufinfo[i]) < 0) {
			perror("VIDIOC_QBUF");
			exit(15);
		}
	}

	if (ioctl(vfd, VIDIOC_STREAMON, &type) < 0) {
		perror("VIDIOC_STREAMON");
		exit(16);
	}
	if (fcntl(vfd, F_SETFL, O_NONBLOCK) < 0)
		perror("F_SETFL");
}

void
end_video_in(void) {
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;

	if (ioctl(vfd, VIDIOC_STREAMOFF, &type) < 0) {
		perror("VIDIOC_STREAMOFF");
		exit(16);
	}

	for (i=0; i<n_buffers; i++)
		munmap(buffers[i].addr, buffers[i].length);
	close(vfd);
}

typedef	u_long YUYV442;

Pixel32
YUV444toRGB888(int y, int u, int v) {
	int r, g, b;
	int c = y-16;
	int d = u - 128;
	int e = v - 128;       

	r = (298 * c           + 409 * e + 128) >> 8;
	g = (298 * c - 100 * d - 208 * e + 128) >> 8;
	b = (298 * c + 516 * d           + 128) >> 8;

	// No idea why this is bgr, not rgb
	return SETRGB(CLIP(b), CLIP(g), CLIP(r));
}

void
YUYVtoRGB(YUYV442 yuyv[IN_H][IN_W/2], Pixel32 rgb[MAX_Y][MAX_X]) {
	int x, y;

	for (y=0; y< MAX_Y; y++) {
		int grab_y = (IN_H - MAX_Y)/2 + y;
		int grab_x_offset = ((IN_W - MAX_X)/2 & ~1);	// starts on even
		YUYV442 *grab_row = &yuyv[grab_y][grab_x_offset];
		Pixel32 *frame_row = &rgb[MAX_Y - y - 1][MAX_X-1];

		for (x=0; x<MAX_X /*MAX_X*/; x+=2) {
			YUYV442 yuyv422 = *grab_row++;
			int Y2  = ((yuyv422 & 0x000000ff));
			int V = ((yuyv422 & 0x0000ff00)>>8);
			int Y1  = ((yuyv422 & 0x00ff0000)>>16);
			int U = ((yuyv422 & 0xff000000)>>24);
			*frame_row-- = YUV444toRGB888(Y2, U, V);
			*frame_row-- = YUV444toRGB888(Y1, U, V);
		}
	}
}

int last_buf = 0;

void
grab_video_in(void) {
	struct v4l2_buffer buf;
	int last_good_buf = -1;

	// skip over queued frames to the latest good one

	while (1) {
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(vfd, VIDIOC_DQBUF, &buf) < 0) {
			if (errno == EAGAIN)
				break;
			perror("VIDIOC_DQBUF");
			continue;
		}
		last_good_buf = buf.index;
		if (ioctl(vfd, VIDIOC_QBUF, &buf) < 0)
			perror("VIDIOC_QBUF");
	}
	if (last_good_buf < 0) {
		return;
	}

	YUYVtoRGB((void *)buffers[last_good_buf].addr, input_frame);
}

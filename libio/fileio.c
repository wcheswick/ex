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
 *	fileio.c - dump and read image files.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <assert.h>

#include <jpeglib.h>
//#include <pam.h>

#include "libutil/util.h"
#include "io.h"


int
write_jpeg_image(char *fn, Pixel (*frame)[MAX_Y][MAX_X]) {
	FILE *jpegout;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int x, y;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if (fn)
		jpegout = fopen(fn, "wb");
	else {
		int fd = mkstemps("XXX.jpeg", 5);
		jpegout = fdopen(fd, "w");
	}
	if (jpegout == NULL) {
		perror("can't open jpeg");
		return 0;
	}

	jpeg_stdio_dest(&cinfo, jpegout);
	cinfo.image_width = MAX_X;      /* image width and height, in pixels */
	cinfo.image_height = MAX_Y;
	cinfo.input_components = 3;     /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
	cinfo.dct_method = JDCT_ISLOW;	/* slow, high accuracy encoding */
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, 100, 1);	/* highest quality encoding, w. baseline */
	jpeg_start_compress(&cinfo, TRUE);

	for (y=0; y<MAX_Y; y++) {
		Pixel *in_row = &((*frame)[MAX_Y - y - 1][0]);
		JSAMPLE row[MAX_X*3];
		JSAMPROW row_pointer[1];
		row_pointer[0] = &row[0];

		for (x=0; x<MAX_X; x++) {
			row[x*3]   = in_row[x].r;
			row[x*3+1] = in_row[x].g;
			row[x*3+2] = in_row[x].b;
		}
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
        jpeg_finish_compress(&cinfo);
        fclose(jpegout);
	jpeg_destroy_compress(&cinfo);
	return 1;
}

/*
 * Decode jpeg image from current place in the given file.
 */
int
decode_jpeg_image(FILE *jpegin, Pixel (*frame)[MAX_Y][MAX_X], int flip) {
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, jpegin);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	/*
	 * available components in cinfo:
	 * output_width            image width and height, as scaled
	 * output_height
	 * out_color_components    # of color components in out_color_space
	 * output_components       # of color components returned per pixel
	 * colormap                the selected colormap, if any
	 * actual_number_of_colors         number of entries in colormap
	 * image_width                  Width and height of image
	 * image_height
	 */


	if (cinfo.output_width != MAX_X || MAX_Y != cinfo.output_height)
		return 0;
	row_stride = cinfo.output_width * cinfo.output_components;

	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		int x, y = cinfo.output_scanline;
		u_char *bp = buffer[0];
		int out_y;

		if (flip)
			out_y = MAX_Y-y-1;
		else
			out_y = y;

		jpeg_read_scanlines(&cinfo, buffer, 1);
		for (x=0; x<MAX_X; x++) {
			Pixel p;
			switch (cinfo.output_components) {
			case 1:
				p.r = p.g = p.b = *bp++;
				p.a = Z;
				break;
			case 3:
				p.r = *bp++;
				p.g = *bp++;
				p.b = *bp++;
				p.a = Z;
				break;
			default:
				return 0;	// beats me
			}
			(*frame)[out_y][x] = p;
		}
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return 1;
}

int
read_jpeg_image(char *fn, Pixel (*frame)[MAX_Y][MAX_X], int flip) {
	FILE *jpegin;
	int rc;

	if ((jpegin = fopen(fn, "rb")) == NULL) {
		fprintf(stderr, "%s: warning, can't open sample file %s\n", prog, fn);
		return 0;
	}

	rc = decode_jpeg_image(jpegin, frame, flip);
	fclose(jpegin);
	return rc;
}

/*
 * Read an image from a file and return its size, and an allocated buffer
 * with the Pixels.  The images are PixMap format.
 */
Pixel *
read_pnm_image(char *fn, int *dx, int *dy) {
	Pixel *image, *ip;
	char buf[100];
	int depth;
	int x, y;
	FILE *f = fopen(fn, "r");

	if (ferror(f)) {
		perror("Open PNM image");
		return 0;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		fprintf(stderr, "read_pnm_image: error reading magic number: %s\n",
			fn);
		return 0;
	}
	if (strcmp(buf, "P6\n")) {
		fprintf(stderr, "read_pnm_image: unexpected magic number: %s, file %s\n", buf, fn);
		return 0;
	}

	do {
		if (fgets(buf, sizeof(buf), f) == NULL) {
			fprintf(stderr, "read_pnm_image: EOF reading dimensions, file %s\n", fn);
			return 0;
		}
	} while (!isdigit(buf[0]));	// skip comments

	if (sscanf(buf, "%d %d\n", dx, dy) != 2) {
		fprintf(stderr, "read_pnm_image: error reading dimensions: %s\n", fn);
		return 0;
	}

	if (fscanf(f, "%d\n", &depth) != 1) {
		fprintf(stderr, "read_pnm_image: error reading depth: %s\n", fn);
		return 0;
	}
	if (depth != 255) {
		fprintf(stderr, "read_pnm_image: unexpected depth: %d in %s\n",
			depth, fn);
		return 0;
	}

	image = (Pixel *)malloc(*dx * *dy * sizeof(Pixel));
	assert(image);	// out of space allocating image

	/* Images are upside down for our layout, so lets fix that here. */

	ip = &image[*dx * *dy];

	for (y = *dy - 1; y>=0; y--) {
		Pixel *pp = &image[y * *dx];

		for (x=0; x < *dx; x++) {
			int r = fgetc(f);
			int g = fgetc(f);
			int b = fgetc(f);
			if (r == EOF || g == EOF || b == EOF) {
				fprintf(stderr, "read_pnm_image: unexpected EOF: %s\n", fn);
				return 0;
			}
			*pp++ = SETRGB(r,g,b);
		}
	}
	return image;
}

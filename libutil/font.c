/*
 * Copyright 2004,2011 Bill Cheswick <ches@cheswick.com>
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
 * Read BDF font files, and render text using them.
 *
 * Based on xc/lib/font/bitmap/bdfread.c, but much simpler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
//#include <rune.h>
#include <errno.h>
#include <assert.h>

#include "libutil/util.h"
#include "libio/io.h"
#include "font.h"

font *current_font = 0;

void
set_font(font *fp) {
	if (fp == 0) {
		fprintf(stderr, "set_font: font missing, aborting\n");
		exit(20);
	}
	current_font = fp;
}

char *file_name;

#define ROW(enc)	(((enc) >> 8) & 0xff)
#define COL(enc)	((enc) & 0xff)

#define LOCALE	"am_ET.UTF-8"

void
init_font_locale(char *loc) {
#ifdef setrunelocale
	if (loc)
		setrunelocale(loc);
	else
		setrunelocale(LOCALE);
#endif
}

static int
prefix(char *line, char *msg) {
	return (strncmp(line, msg, strlen(msg)) == 0);
}

static int
read_header(FILE *f, font *fp) {
	int n;

	if (!get_nonempty_line(f))
		return 0;
	if (!prefix(line, "STARTFONT 2.1")) {
		fprintf(stderr, "font file %s: bad format, not bdf\n", file_name);
		return 0;
	}

	if (!get_nonempty_line(f))
		return 0;
	if (!prefix(line, "FONT ")) {
		fprintf(stderr, "font file %s: font name format error\n", file_name);
		return 0;
	}

	if (!get_nonempty_line(f))
		return 0;
	if ((n=sscanf(line, "SIZE %f%d%d", &fp->size, &fp->dpi_x, &fp->dpi_y)) != 3) {
		fprintf(stderr, "font file %s: font size format error (%d)\n",
			file_name, n);
		return 0;
	}
	if (fp->dpi_x < 1 || fp->dpi_y < 1) {
		fprintf(stderr, "font file %s: sizes must be > 0\n", file_name);
		return 0;
	}

	if (!get_nonempty_line(f))
		return 0;
	if (!prefix(line, "FONTBOUNDINGBOX ")) {
		fprintf(stderr, "font file %s: FONTBOUNDINGBOX missing\n", file_name);
		return 0;
	}

	return 1;
}

static int
read_properties(FILE *f, font *fp) {
	int n, nprops, unknown = 0;

	fp->descent = fp->ascent = -1;

	if (!get_nonempty_line(f))
		return 0;
	if ((n=sscanf(line, "STARTPROPERTIES %d", &nprops)) != 1) {
		fprintf(stderr, "font file %s: STARTPROPERTIES format error\n",
			file_name);
		return 0;
	}

	while (get_nonempty_line(f)) {
		if (prefix(line, "ENDPROPERTIES"))
			break;
		else if (sscanf(line, "POINT_SIZE %d", &fp->point_size) == 1)
			continue;
		else if (sscanf(line, "WEIGHT %d", &fp->weight) == 1)
			continue;
		else if (sscanf(line, "RESOLUTION_X %d", &fp->resx) == 1)
			continue;
		else if (sscanf(line, "RESOLUTION_Y %d", &fp->resy) == 1)
			continue;
		else if (sscanf(line, "X_HEIGHT %d", &fp->xheight) == 1)
			continue;
		else if (sscanf(line, "QUAD_WIDTH %d", &fp->quad_width) == 1)
			continue;
		else if (sscanf(line, "DEFAULT_CHAR %d", &fp->default_char) == 1)
			continue;
		else if (sscanf(line, "FONT_DESCENT %d", &fp->descent) == 1)
			continue;
		else if (sscanf(line, "FONT_ASCENT %d", &fp->ascent) == 1)
			continue;
		else
			unknown++;
	}

	if (fp->descent == -1 || fp->ascent == -1) {
		fprintf(stderr, "font file %s: missing font ascent/descent info\n",
			file_name);
		return 0;
	}
	return 1;
}

int
tohex(int c) {
	if (isdigit(c))
		return c - '0';
	else
		return tolower(c) - 'a' + 10;
}

/*
 * Flip the character upside down as we read it, so it will match our
 * co-ordinate system.
 */
char *
read_bitmap(FILE *f, struct rune_info *ri) {
	int w_bits = ri->bbx_size.x;
	int h;
	int row;

	h = ri->bbx_size.y;
	ri->bytes_per_row = (w_bits+7) / 8;

	if (ri->bytes_per_row * h > 0) {
		ri->bitmap = (u_char *)malloc(ri->bytes_per_row * h);
		assert(ri->bitmap);	// allocating memory for a rune bitmap
	} else
		ri->bitmap = 0;		// empty bitmap.  Hmm.

	for (row=0; row<h; row++) {
		byte *bp = &ri->bitmap[(h-row-1)*ri->bytes_per_row];
		int l, b;
		char *cp = line;

		if (!get_nonempty_line(f))
			break;
		l = strlen(line);
		if (l & 1)
			return "Odd number of characters in hex encoding";
		if (l/2 != ri->bytes_per_row)
			return "Wrong string length";
		for (b=0; b<ri->bytes_per_row; b++, cp += 2) {
			if (!isxdigit(cp[0]) || !isxdigit(cp[1]))
				return "Non-hex digit encountered";
			*bp++ = tohex(cp[0]) << 4 | tohex(cp[1]);
		}
	}
	if (row != 0) {
		if (!get_nonempty_line(f))
			return "EOF reading bitmap";
	}

	return 0;
}

static int
read_characters(FILE *f, font *fp) {
	int i, n;
	int row, col;
	int nchar;

	if (!get_nonempty_line(f))
		return 0;
	if ((n=sscanf(line, "CHARS %d", &nchar)) != 1 || nchar < 1) {
		fprintf(stderr, "font file %s: bad CHARS\n",
			file_name);
		return 0;
	}

	for (row=0; row<CHAR_ROWS; row++)
		for (col=0; col<CHAR_COLS; col++)
			fp->chars[row][col].bitmap = 0;

	for (i=0; i<nchar && get_nonempty_line(f); i++) {
		int enc, enc2;
		int wx, wy;
		int dwx, dwy;
		int bw, bh, bl, bb;
		struct rune_info *ri;
		char *err;
		
		char char_name[sizeof(line)];

		if ((n=sscanf(line, "STARTCHAR %s", char_name)) < 1) {
			fprintf(stderr, "font file %s: chars missing (%d): %s\n",
				file_name, n, line);
			return 0;
		}

		if (!get_nonempty_line(f))
			return 0;
		if ((n=sscanf(line, "ENCODING %d %d", &enc, &enc2)) < 1) {
			fprintf(stderr, "font file %s: char %s bad ENCODING\n",
				file_name, char_name);
			return 0;
		}
		if (enc < -1 || (n == 2 && enc2 < -1)) {
			fprintf(stderr, "font file %s: char %s bad ENCODING value\n",
				file_name, char_name);
			return 0;
		}
		if (n == 2 && enc == -1)
			enc = enc2;
		if (enc == -1)
			continue;	// ignore it.  X calls it "extra". I don't think we care
		if (enc > MAXENCODING) {
			fprintf(stderr, "font file %s: char %s Bad ENCODING value\n",
				file_name, char_name);
			return 0;
		}
		row = ROW(enc);
		col = COL(enc);
		if (fp->chars[row][col].bitmap) {
			fprintf(stderr, "font file %s: char %s duplicate character\n",
				file_name, char_name);
			return 0;
		}

		// scaled width.  We ignore this.

		if (!get_nonempty_line(f))
			return 0;
		if ((n=sscanf(line, "SWIDTH %d %d", &wx, &wy)) != 2) {
			fprintf(stderr, "font file %s: char %s bad SWIDTH\n",
				file_name, char_name);
			return 0;
		}
		if (wy != 0)  {
			fprintf(stderr, "font file %s: char %s bad SWIDTH, y must be 0\n",
				file_name, char_name);
			return 0;
		}

		// device pixel width, the distance from the start of this
		// character to the start of the next.

		if (!get_nonempty_line(f))
			return 0;
		if ((n=sscanf(line, "DWIDTH %d %d", &dwx, &dwy)) != 2) {
			fprintf(stderr, "font file %s: char %s bad DWIDTH\n",
				file_name, char_name);
			return 0;
		}
		if (dwy != 0)  {
			fprintf(stderr, "font file %s: char %s bad DWIDTH, y must be 0\n",
				file_name, char_name);
			return 0;
		}

		// bounding box of the black pixels, and their offset.

		if (!get_nonempty_line(f))
			return 0;
		if ((n=sscanf(line, "BBX %d %d %d %d", &bw, &bh, &bl, &bb)) != 4) {
			fprintf(stderr, "font file %s: char %s bad BBX\n",
				file_name, char_name);
			return 0;
		}
		if (bh < 0 || (bw < 0)) {
			fprintf(stderr, "font file %s: char %s negative-sized bitmap\n",
				file_name, char_name);
			return 0;
		}

		if (!get_nonempty_line(f))
			return 0;
		if (prefix(line, "ATTRIBUTES")) {	// ignore
			if (!get_nonempty_line(f))
				return 0;
		}
		if (!prefix(line, "BITMAP")) {
			fprintf(stderr, "font file %s: char %s BITMAP missing\n",
				file_name, char_name);
			return 0;
		}

		ri = &(fp->chars[row][col]);
		ri->bbx_size = (Point){bw, bh};
		ri->bbx_offset = (Point){bl, bb};
		ri->dwidth = (Point){dwx, dwy};
		if ((err = read_bitmap(f, ri))) {
			fprintf(stderr, "font file %s: char %s bitmap error: %s\n",
				file_name, char_name, err);
			return 0;
		}

		if (!prefix(line, "ENDCHAR")) {
			fprintf(stderr, "font file %s: char %s ENDCHAR unexpected\n",
				file_name, char_name);
			return 0;
		}
	}
	return 1;
}

font *
load_font(char *fn) {
	char buf[500];
	FILE *f;
	font *fp;

	if (strchr(fn, '.') == (char *)0) {
		snprintf(buf, sizeof(buf), "%s.bdf", fn);
		fn = buf;
	}

	f = fopen(fn, "r");

	/*
	 * During debugging, we look all over for the font directory.
	 */
	if (f == NULL) {
		char *fd = getenv("FONTS");
		if (fd) {
			char buf[512];
			snprintf(buf, sizeof(buf), "%s/%s", fd, fn);
			f = fopen(buf, "r");
		}
	}

#ifdef FONTDIR
	if (f == NULL) {
		char buf[512];
		snprintf(buf, sizeof(buf), "%s/%s", FONTDIR, fn);
		f = fopen(buf, "r");
	}
#endif

	if (f == NULL) {
		char *path = find_file(fn, "lib/fonts");
		if (path)
			f = fopen(path, "r");
	}
		
	if (f == NULL) {
		fprintf(stderr, "opening font file '%s', aborting: %s\n", fn,
			strerror(errno));
		exit(1);
	}

	file_name = fn;
	fp = (font *)malloc(sizeof(font));
	if (fp == 0) {
		fprintf(stderr, "read_font: out of memory");
		return 0;
	}

	if (!read_header(f, fp)) {
		free(fp);
		return 0;
	}

	if (!read_properties(f, fp)) {
		free(fp);
		return 0;
	}

	if (!read_characters(f, fp)) {
		free(fp);
		return 0;
	}

	return fp;
}

void
dump_row(byte *bitmap, int len) {
	int i;
	for (i=0; i<len; i++) {
		int bit = (bitmap[i/8] >> (7-(i%8))) & 1;
		fprintf(stderr, "%s", bit ? "X" : ".");
	}
	fprintf(stderr, "\n");
}

#define MAXRUNESPERLINE	300

/*
 * Render a string into a of bitmap information.  This calls
 *	<report_render_size> which returns the given rectangle, or the
 *		size of the given string if the size of the given rectangle is zero
 *	<bitmap_row> which delivers the bits, one row per call.
 */
static void
render_string_line(Rectangle r, utf8 *str, void report_render_size(Rectangle r),
    void bitmap_row(Point p, int len, u_char *bitmap), int centerx) {
	u_char bitmap[800];	// plenty big, 800*sizeof(u_char) bits of text
	Rectangle size = {{0, 0}, {0, 0}};
	rune_t runes[MAXRUNESPERLINE];
	int nrunes=0;
	const char *ep;
	char *cp = str;
	int len = strlen(str);
	int bitmap_bytes_per_row;
	Point next = (Point){0,0};
	int row;
	int start_x;

	if (!current_font) {
		fprintf(stderr, "render_string_line: no current font\n");
		exit(21);
	}

	/* assemble the rune list in 'runes', and keep track of sizes */

	size.min.y = -current_font->descent;
	size.max.y = current_font->ascent;
	while (*cp) {
		struct rune_info *ri;
#ifdef sgetrune
		rune_t rune = sgetrune(cp, len, &ep);
		int n = ep - cp;
#else
		rune_t rune = *cp++;
		int n = 1;
		ep = cp;
#endif

		if (ep == NULL || n == 0 || nrunes == MAXRUNESPERLINE) {
			fprintf(stderr, "Render_string rendering problem: %s\n", str);
			break;	// not advancing
		}
		cp = (char *)ep;
		len -= n;
		if (rune > MAXENCODING)
			rune = current_font->default_char;
		ri = &(current_font->chars[ROW(rune)][COL(rune)]);
		if (ri->bitmap == 0) {
			rune = current_font->default_char;
			ri = &(current_font->chars[ROW(rune)][COL(rune)]);
		}
		runes[nrunes++] = rune;

		if (next.x + ri->bbx_size.x + ri->bbx_offset.x > size.max.x)
			size.max.x = next.x + ri->bbx_size.x + ri->bbx_offset.x;
		next.x += ri->dwidth.x + ri->bbx_offset.x;
		next.y += ri->dwidth.y + ri->bbx_offset.y;
	}
//size.max.x += 100;
	bitmap_bytes_per_row = (DX(size)+7) / 8;
//fprintf(stderr, "bitmap_bytes_per_row = %d\n", bitmap_bytes_per_row);
	assert(bitmap_bytes_per_row < sizeof(bitmap));
	if (DX(r) == 0) {
		r.min.x = size.min.x;
		r.max.x = size.max.x;
	}
	if (DY(r) == 0) {
		r.min.y = size.min.y;
		r.max.y = size.max.y;
	}
	if (report_render_size)
		report_render_size(r);

	if (centerx)
		start_x = r.min.x + (DX(r) - DX(size)) / 2;
	else
		start_x = r.min.x;
//fprintf(stderr, "str = %s  len=%d\n", str, DX(r));
//fprintf(stderr, "startx = %d\n", start_x);
//fprintf(stderr, "r = %d,%d  %d,%d\n", r.min.x, r.min.y, r.max.x, r.max.y);
//fprintf(stderr, "nrunes = %d\n", nrunes);

	for (row=size.min.y; row<size.max.y; row++) {
		int i;
		int rune_x = start_x;
		memset(bitmap, 0, sizeof(bitmap));

		for (i=0; i<nrunes; i++) {
			rune_t rune = runes[i];
			struct rune_info *ri =
				&(current_font->chars[ROW(rune)][COL(rune)]);
			int j, x, shift, mask, len;
			byte *rbp, *out;

//if (row==size.min.y)
//fprintf(stderr, "rune=%c  dwidth=%d,%d  size=%d,%d  offset=%d,%d bpr=%d\n",
//	rune, ri->dwidth.x, ri->dwidth.y, ri->bbx_size.x, ri->bbx_size.y,
//	ri->bbx_offset.x, ri->bbx_offset.y, ri->bytes_per_row);
			/*
			 * See if no bits in this rune for this row.
			 */
			if (row < ri->bbx_offset.y || row >= ri->bbx_size.y + ri->bbx_offset.y)
				continue;

			rbp = &ri->bitmap[row * ri->bytes_per_row];
			x = rune_x + ri->bbx_offset.x;

			shift = x % 8;
			mask = ((1 << shift)-1) << (8 - shift);
			out = &bitmap[x/8];
			len = (ri->bbx_size.x + (8-1))/8;

			for (j = 0; j<len; j++) {
				*out++ |= *rbp >> shift;
				*out = ((*rbp++) << (8-shift)) & mask;
			}
			rune_x += ri->dwidth.x + ri->bbx_offset.x;
		}
//dump_row(bitmap, DX(r));
		bitmap_row((Point){r.min.x, r.min.y + current_font->descent + row},
		    DX(r), bitmap);
	}
//exit(13);
}


Pixel render_color;

/*
 * This is a callback with a string of bits.  We set the colors on
 * the screen accordingly.
 */
#define MAXWIDTH	2000

Rectangle ws_r;

static void
write_screen_row(Point p, int len, u_char *bitmap) {
	PixMap line;
	Pixel prow[MAXWIDTH], *pp = prow;
	int bits_left = 8;
	int i;
	int size = DX(ws_r);

	line.r = (Rectangle){{0,0}, {len, 1}};
	line.pm = prow;

	if (len > MAXWIDTH)
		len = MAXWIDTH;
	for (i=0; i<len; i++) {
		if (size > 0) {
			bits_left--;
			if ((bitmap[0] >> bits_left) & 1)
				*pp++ = render_color;
			else
				*pp++ = Black;
			if (bits_left == 0) {
				bitmap++, bits_left = 8;
			}
			size--;
		} else
			*pp = Black;
	}
	write_pixmap(p, (Rectangle){{0,0}, {len,1}}, line);
}

void
write_string(Rectangle r, Pixel color, utf8 *str) {
	ws_r = r;
	render_color = color;
	render_string(r, str, 0, write_screen_row, 0, 0);
}

void
write_centered_string(Rectangle r, Pixel color, utf8 *str) {
	ws_r = r;
	render_color = color;
	render_string(r, str, 0, write_screen_row, 1, 0);
}

PixMap *render_pixmap = 0;

static void
create_pixmap(Rectangle r) {
	static PixMap pm;
	render_pixmap = &pm;
	render_pixmap->pm = (Pixel *)malloc(DX(r)*sizeof(Pixel)*DY(r));
	assert(render_pixmap->pm);	/* allocating bitmap */
	render_pixmap->r = r;
}

/*
 * This is a callback with a string of bits.  We set the requested color
 * when the font's bit is set, and leave it alone otherwise.
 */
static void
write_pixmap_row(Point p, int len, u_char *bm) {
	int bits_left = 8;
	int i;
	Pixel *pp = PIXMAPADDR((*render_pixmap), p.x, p.y);

	if (p.y < render_pixmap->r.min.y || p.y >= render_pixmap->r.max.y)
		return;

	for (i=0; i<len && i+p.x < render_pixmap->r.max.x; i++) {
		int set_pixel = 0;
		bits_left--;
		if ((bm[0] >> bits_left) & 1)
			set_pixel = 1;
		if (bits_left == 0) {
			bm++, bits_left = 8;
		}
		if (i+p.x < render_pixmap->r.min.x)
			continue;
		if (set_pixel)
			*pp = render_color;
		pp++;
	}
}

/*
 * XXX - untested:
 */
PixMap *
string_to_pixmap(Rectangle r, Pixel color, utf8 *str, int cx, int cy) {
	render_color = color;
	render_string(r, str, create_pixmap, write_pixmap_row, cx, cy);
	return render_pixmap;
}

void
string_on_pixmap(PixMap *pm, utf8 *str, Pixel color, int cx, int cy) {
	render_color = color;
	render_pixmap = pm;
	render_string(pm->r, str, 0, write_pixmap_row, cx, cy);
}

/*
 * rbm is set to point at the current line entry before we render the line,
 * so these routines will know where to deposit the information and bits
 * during rendering.
 */
static Bitmap *rbm;
static int bitmap_bytes_per_row;

static void
set_runeline_row(Point p, int len, u_char *bm) {
	int bytes = (len + 7) >> 3;

	assert(p.y*bitmap_bytes_per_row + bytes <= bitmap_bytes_per_row * DY(rbm->r));
	memmove(BITMAPLINEADDR((*rbm), p.y), bm, bytes);
}

static void
make_runeline_bitmap(Rectangle r) {
	int n = DY(r)*(8+DX(r))/8;

	rbm->bm = (u_char *)malloc(n);
	assert(rbm->bm);	/* make_runeline_bitmap allocating bitmap */
	rbm->r = r;
	memset(rbm->bm, 0, n);
	bitmap_bytes_per_row = (DX(r) + 7) >> 3;
}

/*
 * Lines of text in 'text' are separated by "|" or "\n".  Stack
 * these bitmaps into the given pixel buffer, centered, and if it doesn't fit,
 * clipped.
 */

#define MAXLINES	100

void
render_string(Rectangle r, utf8 *pstr, void report_render_size(Rectangle r),
    void bitmap_row(Point p, int len, u_char *bitmap), int centerx, int centery) {
	utf8 *ep, *str = strdup(pstr);
	utf8 *cp = str;
	int i, nlines=0, rows=0;
	int skipy;
	int width, maxwidth = 0;
	int in_row=0, in_line;
	int out_startx=0, lenx=0;
	int out_row;
	Bitmap line[MAXLINES];
	Rectangle empty = (Rectangle){{0,0}, {0,0}};

	do {	int h;
		ep = strpbrk(cp, "|\n");
		if (ep)
			*ep = '\0';
		rbm = &line[nlines];
		render_string_line(empty, cp, make_runeline_bitmap,
			set_runeline_row, centerx);
		h = DY(line[nlines].r);
		if (h == 0)
			continue;	/* ignore lines with no height */
		rows += h;
		width = DX(line[nlines].r);
		if (width > maxwidth)
			maxwidth = width;
		nlines++;
		if (ep)
			cp = ep+1;
	} while (ep && nlines < MAXLINES);
	free(str);
	if (nlines == MAXLINES)
		fprintf(stderr, "place_text: warning, too many lines in text: %s\n",
			pstr);

	/*
	 * Compute the size of the resulting PixMap, if the caller didn't
	 * specify one.
	 */
	if (DX(r) == 0)
		r.max.x = r.min.x + maxwidth;
	if (DY(r) == 0)
		r.max.y = r.min.y + rows;
	if (report_render_size)
		report_render_size(r);

	/*
	 * Now run through the rows, sending out the scan lines, and freeing
	 * the memory used by the rows. We may have to deal with centering, and
	 * oversize stuff, in both x and y direction.
	 */


	/*
	 * Adjust for vertical centering or vertical overflow.
	 */
	if (!centery)
		skipy = 0;
	else
		skipy = (DY(r) - rows)/2;

	out_row = r.max.y - 1;
	if (skipy > 0) {
		out_row -= skipy;		/* center in vertical space */
		skipy = 0;
	}

	in_line = -1;
	for (; out_row >= r.min.y; out_row--) {
		if (in_line < 0 || in_row <= line[in_line].r.min.y) {
			/* go up to the next input bitmap line */

			int width;

			if (++in_line >= nlines)
				break;
			assert(DY(line[in_line].r) > 0);
			in_row = line[in_line].r.max.y - 1;

			/*
			 * center the new line in X direction, if desired.
			 * We don't bother trying to center an overwidth line.
			 */
			width = DX(line[in_line].r);
			if (width == DX(r) || !centerx || width > DX(r)) {
				out_startx = r.min.x;
				lenx = DX(r);
				if (width < lenx)
					lenx = width;
			} else {			/* center a short line */
				out_startx = r.min.x + (DX(r) - width)/2;
				lenx = DX(line[in_line].r);
			}
		} else
			in_row--;
		if (skipy < 0) {	/* we are skipping top lines in an overfull vbox */
			skipy++;
			continue;
		}
		assert(skipy == 0);
		bitmap_row((Point){out_startx,out_row}, lenx, BITMAPLINEADDR(line[in_line], in_row));
	}
	for (i=0; i<nlines; i++) {	/* recover bitmap memory */
		free(line[i].bm);
	}
}

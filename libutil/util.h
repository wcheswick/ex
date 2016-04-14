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


//typedef u_char utf8;
typedef char utf8;

typedef long long l_time_t;

typedef struct Rectangle	Rectangle;
typedef struct Point		Point;

struct Point {
	int	x, y;
};

struct Rectangle {
	Point	min, max;
};


#define DX(r)	((r).max.x - (r).min.x)
#define	DY(r)	((r).max.y - (r).min.y)

#ifdef notdef

struct Pixel16 {	// #if BYTE_ORDER == LITTLE_ENDIAN
	u_short a:1;	/*alpha...not used */
	u_short	b:5;
	u_short	g:5;
	u_short	r:5;
};

typedef struct Pixel24	Pixel24;
struct Pixel24 {
	u_char b,g,r;
};
#endif

typedef struct Pixel32	Pixel32;
struct Pixel32 {
	u_char b,g,r,a;
};

/* In these routines, we are using only the 32-bit pixels. */

typedef Pixel32 Pixel;
typedef	u_char	channel;

typedef struct PixMap PixMap;	// spelled this way to avoid conflict with X
struct PixMap {
	Rectangle r;
	Pixel *pm;
};

/* Compute the address of p[x][y] */

#define PIXMAPADDR(pixmap,xc,yc)  &(pixmap.pm[((yc)-pixmap.r.min.y)*DX(pixmap.r) + (xc)])

/*
 * bitmaps are arrays of pixels from the font routines.
 */
typedef struct Bitmap Bitmap;
struct Bitmap {
	Rectangle r;
	u_char *bm;
};

/* Compute the address of b[x][y] */

#define BITMAPLINEADDR(bitmap,yc)  &(bitmap.bm[((yc)-bitmap.r.min.y)*((DX(bitmap.r)+7) >> 3)])

extern	int debug;

/* In util.c */
extern	int ptinrect(Point p, Rectangle r);
extern	int rectinrect(Rectangle r1, Rectangle r2);
extern	Rectangle inset(Rectangle r, int i);
extern	utf8 *lookup(char *name, char *name2, utf8 *default_label);
extern	int get_nonempty_line(FILE *f);
extern	void load_config(char *fn);
extern	void make_PixMap(PixMap *pm, Rectangle r, Pixel color);
extern	char *find_file(char *fn, char *dir);
extern	l_time_t rtime(void);
extern	long long dt_ms(struct timeval now, struct timeval before);
extern	int elapsed_time(struct timeval t1, struct timeval t2);
extern	int over_fps(int fps);
extern	int set_background(char *fn, int w, int h);
extern	void clear_screen_to_background(void);
extern	void clear_to_background(Rectangle r);

extern	char line[2000];

/* stupid loading warning messages for these routines...kill 'em in util.c */
extern	char *gets(char *);
extern	char *mktemp(char *);
extern	char *tmpnam(char *);
extern	char *tempnam(const char *, const char *);
extern	void f_prealloc(void);

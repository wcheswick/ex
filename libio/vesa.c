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
 * vesa - budding support for the vesa vga card.  Should be much simpler than vgl,
 * which I am getting rid of, slowly.
 *
 * These vesa routines implement the origin in the lower left hand corner
 * of the screen, where God intended it to be.
 */

DEPRECATED: we are all X now

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// FT_PIXEL_MODE_LCD

#include "util.h"
#include "io.h"
#include "vgl.h"
#include "vesa.h"

#define	VESA_MAX_X	1280

struct video_info vesa_vid_mode;

int vesa_initialized = 0;
int vesa_max_x;
int vesa_max_y;
int vesa_bytes_per_pixel;
int vesa_y_stride;	/* bytes per displayed row */

void *vesa_buf;		/* our VGA/VESA display memory */

#define	VESA_ADDR(x,y)	((Pixel *)(vesa_buf + VESA_Y(y)*vesa_y_stride + (x)*vesa_bytes_per_pixel))

sprite mouse_sprite = {Undef};

/*
 * Adopted from vidcontrol.c.  Find the interesting VESA modes.  In our
 * case, we are only using the x32 bit depths.  The VESA VGA mode numbers
 * vary, but we have a kernel mod that lets us ask for any mode we want.
 * If it is a legal VESA mode for this card, we get it.
 */
static struct video_info
vesa_scan_modes(int w, int h) {
	struct 	vid_info vi;
	struct	video_info info, best_i, second_best_i;
	int mode;
	int depth_wanted = sizeof(Pixel)*8;

	best_i.vi_mode = second_best_i.vi_mode = 0;

	vi.size = sizeof(vi);
	if (ioctl(0, CONS_GETINFO, &vi) < 0) {
		if (errno == ENOTTY)
			fprintf(stderr, "Not a VGA VESA console, aborting\n");
		else
			perror("vesa_scan_modes: CONS_MODEINFO, aborting");
		return best_i;
	}
	
	for (mode = 0; mode < M_VESA_MODE_MAX; ++mode) {
		info.vi_mode = mode;
		if (ioctl(0, CONS_MODEINFO, &info))
			continue;
		if (info.vi_mode != mode)
			continue;
		if ((info.vi_flags & V_INFO_GRAPHICS) == 0)
			continue;
		if (info.vi_depth < 24 || info.vi_width < 640 ||
		    info.vi_height < 480)	// we don't care
			continue;

		if (w == info.vi_width && h == info.vi_height) {
			if (info.vi_depth  == depth_wanted) {
				best_i = info;
			} else if (info.vi_depth > depth_wanted)
				second_best_i = info;
		}
		printf("%3d (0x%03x)   ", mode, mode);
		assert(info.vi_planes == 1);
		printf("%dx%dx%d  	",
			info.vi_width, info.vi_height, info.vi_depth);
    		printf("%dk", (int)info.vi_buffer_size/1024);
		if (best_i.vi_mode == mode) {
			printf(" *best mode*\n");
		} else if (second_best_i.vi_mode == mode)
			printf(" *second-best* mode\n");
		else
			printf("\n");
	}
	/*
	 * Actually, at this point we insist on the best, with no second
	 * best.  (E.g. for an internal pixel size, I was going to allow
	 * either x24 or x32 depths, and eat the conversion time.  But it
	 * is much simpler, and fast enough, to use x32 for everything.  And
	 * Moore's law will only make this better.
	 */
	if (best_i.vi_mode == 0)
		best_i = second_best_i;
	fflush(stdout);
	return best_i;
}

void
vesa_end(void) {
	VGLEnd();
}

/*
 * The area under the sprite was changed.  We can put the sprite back now.
 */
static void
sprite_display(sprite *s) {
	Pixel32 *backp= s->back;
	Pixel32 *spritep = s->image;
	int dx  = DX(s->behind);
	int by, uy;

	if (dx <= 0)
		return;

	/* we go backwards because the sprite is stored from the UL corner */
	for (by=s->behind.max.y-1, uy=s->used.max.y-1; by >= s->behind.min.y;
	    by--, uy--) {
		Pixel *bufp = 0;
		Pixel buf[dx];
		int i;

		bufp = VESA_ADDR(s->behind.min.x, by);

		memmove(backp, bufp, dx*vesa_bytes_per_pixel); /* backup screen line */

		/*
		 * compute screen + sprite.  We use the alpha channel in
		 * the Pixel32 definition of a sprite to mix the two. We construct
		 * the line into local memory, and then copy the whole thing
		 * into the frame buffer, which seems to be inefficient for
		 * 3-byte transfers of the 24-bit pixels we used to support.
		 * This is dumb...24-bit pixels are losers and we don't need
		 * to support them ever again.
		 */
		for (i=0; i<dx; i++) {
			Pixel32 p = *spritep++;
			buf[i].r = (p.a*p.r + (255 - p.a)*backp[i].r)/255;
			buf[i].g = (p.a*p.g + (255 - p.a)*backp[i].g)/255;
			buf[i].b = (p.a*p.b + (255 - p.a)*backp[i].b)/255;
		}
		memmove(bufp, buf, sizeof(buf));
		backp += dx;
	}
}

/*
 * Remove the sprite from the display.
 */
void
sprite_off(sprite *s) {
	Pixel32 *backp;
	int dx, by;

	if (s->status != On)
		return;

	backp = s->back;
	dx = s->behind.max.x - s->behind.min.x;
	if (dx <= 0)
		return;
	for (by=s->behind.max.y-1; by >= s->behind.min.y; by--) {
		memmove(VESA_ADDR(s->behind.min.x, by), backp,
		    dx*vesa_bytes_per_pixel); /* restore screen */
		backp += dx;
	}
	s->status = Off;
};

/*
 * Place the sprite on the display.
 */
void
sprite_on(sprite *s, Point p) {
	Point origin;

	if (s->status != Off)
		return;

	origin = (Point){p.x - s->offset.x, p.y - s->offset.y};
	s->used = (Rectangle){{0,0}, {s->size.x, s->size.y}};
	s->behind = (Rectangle){origin, {origin.x + s->size.x, origin.y +
		s->size.y}};

	assert(vesa_initialized);
	/*
	 * See if the sprite is completely off the screen.
	 */
	if (s->behind.min.x >= vesa_max_x || s->behind.min.y >= vesa_max_y ||
	    s->behind.max.x <= 0 || s->behind.max.y <= 0)
		return;

	/*
	 * Now check for clipping by the edges of the screen.
	 */
	if (s->behind.min.x < 0) {
		int dx = -(s->behind.min.x);
		s->behind.min.x = 0;		// clip screen area...
		s->used.min.x += dx;		// ... and the part of the sprite used
	}
	if (s->behind.min.y < 0) {
		int dy = -(s->behind.min.y);
		s->behind.min.y = 0;		// clip screen area...
		s->used.min.y += dy;		// ... and the part of the sprite used
	}
	if (s->behind.max.x > vesa_max_x) {
		int dx = s->behind.max.x - vesa_max_x;
		s->behind.max.x = vesa_max_x;	// clip screen area...
		s->used.max.x -= dx;		// ... and the part of the sprite used
	}
	if (s->behind.max.y > vesa_max_y) {
		int dy = s->behind.max.y - vesa_max_y;
		s->behind.max.y = vesa_max_y;	// clip screen area...
		s->used.max.y -= dy;		// ... and the part of the sprite used
	}

	sprite_display(s);
	s->status = On;
};


/*
 * Recover the memory from the sprite
 */
void
sprite_clean(sprite *s) {
	if (s->status == Undef)
		return;
	assert(s->status != On);
	free(s->back);
	free(s->image);
	s->status = Undef;
};


char *
vesa_init(int w, int h) {
	static char buf[100];
	int rc;
	int vgl_mode;

	vesa_vid_mode = vesa_scan_modes(w,h);

	if (vesa_vid_mode.vi_mode == 0)
		exit(1);

	vgl_mode = _IO('V', vesa_vid_mode.vi_mode - M_VESA_BASE);
	if (getenv("CHECK")) {
		fprintf(stderr, "CHECK, exiting: 0x%.08x\n", vgl_mode);
		exit(2);
	}

	if ((rc = VGLInit(vgl_mode)) < 0) {
		snprintf(buf, sizeof(buf), "%s: VGLInit error %d\n", prog, rc);
		return buf;
	}

	vesa_buf = VGLDisplay->Bitmap;
fprintf(logfd, "*** vesa_buf = %p, w=%d h=%d\n", vesa_buf, w, h);
	vesa_max_x = w;
	vesa_max_y = h;
	vesa_bytes_per_pixel = vesa_vid_mode.vi_depth/8;
	vesa_y_stride = vesa_max_x*vesa_bytes_per_pixel;
	vesa_initialized = 1;
	return 0;
}

#define EASY_VID	(sizeof(Pixel) == 4 || sizeof(Pixel) == vesa_bytes_per_pixel)

void
in_to_vid(Pixel p, Pixel32 *po) {
	if (EASY_VID)
		memmove(po, &p, sizeof(Pixel));
	else {
		po->r = p.r;
		po->g = p.g;
		po->b = p.b;
	}
}

Point lastmouse = {-1,-1};

void
vesa_show_cursor(void) {
	if (mouse_sprite.status == Off)
		sprite_on(&mouse_sprite, (Point){lastmouse.x, VESA_Y(lastmouse.y)});
}

void
vesa_hide_cursor(void) {
	sprite_off(&mouse_sprite);
}

/*
 * Hide the mouse if it is on, and in the given rectangle.  Return code
 * tells restore_mouse if it should be turned on again.
 */
int
vesa_check_mouse(Rectangle r) {
	if (mouse_sprite.status == On &&
	    rectinrect(r, mouse_sprite.behind)) {
		vesa_hide_cursor();
		return 1;
	}
	return 0;
}

void
vesa_restore_mouse(int on) {
	if (on)
		vesa_show_cursor();
}

/*
 * The rectangle has the origin in the lower left.  We have to adjust for the
 * VGA origin in the upper left.
 */
void
vesa_fill_box(Rectangle r, Pixel color) {
	int x, y, t, ms;
	int nx = DX(r);
	Pixel line[VESA_MAX_X];	/* space for our line */

	if (r.min.x > r.max.x) { t = r.min.x; r.min.x = r.max.x; r.max.x = t; }
	if (r.min.y > r.max.y) { t = r.min.y; r.min.y = r.max.y; r.max.y = t; }

	assert(vesa_initialized);
	assert(r.min.x >= 0);
	assert(r.min.y >= 0);
	assert(r.max.x <= vesa_max_x);
	assert(r.max.y <= vesa_max_y);

	ms = vesa_check_mouse(r);

	for (x=0; x<nx; x++)
		line[x] = color;

	for (y=r.min.y; y<r.max.y; y++)
		memmove(VESA_ADDR(r.min.x, y), line,
		    vesa_bytes_per_pixel*nx);
	vesa_restore_mouse(ms);
}

void
vesa_draw_box(Rectangle r, Pixel color) {
	int x, y, t, ms;
	int nx = DX(r);
	Pixel line[VESA_MAX_X];	/* space for our line */

	if (r.min.x > r.max.x) { t = r.min.x; r.min.x = r.max.x; r.max.x = t; }
	if (r.min.y > r.max.y) { t = r.min.y; r.min.y = r.max.y; r.max.y = t; }

	assert(vesa_initialized);
	assert(r.min.x >= 0);
	assert(r.min.y >= 0);
	assert(r.max.x <= vesa_max_x);
	assert(r.max.y <= vesa_max_y);

	ms = vesa_check_mouse(r);

	for (x=0; x<nx; x++)
		line[x] = color;

	memmove(VESA_ADDR(r.min.x, r.min.y), line, vesa_bytes_per_pixel*nx);
	memmove(VESA_ADDR(r.min.x, r.max.y-1), line, vesa_bytes_per_pixel*nx);

	for (y=r.min.y; y<r.max.y; y++)
		*((Pixel *)(VESA_ADDR(r.min.x,y))) =
			*((Pixel *)(VESA_ADDR(r.max.x-1,y))) = color;
	vesa_restore_mouse(ms);
}

/*
 * Clear the whole screen to a given color.
 */
void
vesa_clear(Pixel color) {
	Rectangle screen = (Rectangle){{0,0}, {vesa_max_x,vesa_max_y}};
	int ms;

	assert(vesa_initialized);
	ms = vesa_check_mouse(screen);
	vesa_fill_box(screen, color);
	vesa_restore_mouse(ms);
}

void
vesa_put_line(Point p, Pixel *line, int n_pixels) {
	int ms;

	assert(vesa_initialized);
	ms = vesa_check_mouse((Rectangle){p, {p.x+n_pixels, p.y+1}});
	memmove(VESA_ADDR(p.x,p.y), line, vesa_bytes_per_pixel*n_pixels);
	vesa_restore_mouse(ms);
}

/*
 * Allocate memory for a sprite and its screen backing store.
 */
void
sprite_create(sprite *s, int w, int h) {
	assert(s->status == Undef);

	s->image = (Pixel32 *)malloc(w*sizeof(Pixel32)*h);
assert(vesa_bytes_per_pixel);
	s->back = (Pixel *)malloc(w*h*vesa_bytes_per_pixel);
	assert(s->image);	// out of memory
	assert(s->back);	// out of memory
	s->status = Off;
}

u_char arrow_cursor[] = {
	0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff,
	0xd8, 0x9c, 0x8c, 0x0c, 0x06, 0x06, 0x03, 0x03,
};

/*
 * Set the mouse sprite from a bitmap of and and ors. The width is in bytes,
 * the height in lines.
 */
void
vesa_setmouse(int w, int h, u_char *cursor_mask, Pixel color) {
	int i, j;
	u_char *cp;
	Pixel32 *ip;

	sprite_off(&mouse_sprite);
	sprite_clean(&mouse_sprite);

	if (cursor_mask == 0) {
		cursor_mask = arrow_cursor;
		w = 1;
		h = sizeof(arrow_cursor);
		color = White;
	}

	sprite_create(&mouse_sprite, w*8, h);
	ip = mouse_sprite.image;

	/*
	 * Convert the bit mask to a proper alpha-colored image.
	 */
	cp = cursor_mask;
	for (i=0; i<w*h; i++, cp++) {
		for (j=0; j<8; j++, ip++) { /* each bit in the mask os a pixel */
			ip->r = color.r;
			ip->g = color.g;
			ip->b = color.b;
			if (((*cp) >> (7-j)) & 1) 
				ip->a = 255;
			else
				ip->a = 0;
		}
	}
	mouse_sprite.size = (Point){w*8, h};
	mouse_sprite.status = Off;
	mouse_sprite.offset = (Point){0,h-1};	/* points to UL corner */
}

void
vesa_mouseto(Point p) {
	if (p.x == lastmouse.x && p.y == lastmouse.y)
		return;
	lastmouse = p;
	if (mouse_sprite.status != On)
		return;
	vesa_hide_cursor();
	vesa_show_cursor();
}

/*
 * Write the current screen out to the given file in ppm format.
 *
 * This is not a critical routine, so errors aren't fatal.
 */
int
vesa_dump_screen(char *fn) {
	FILE *f;
	Pixel *p = VESA_ADDR(0,vesa_max_y-1);
	int x=0, y;

	assert(vesa_initialized);
	if (fn)
		f = fopen(fn, "wb");
	else {
		do {
			char buf[1000];
			snprintf(buf, sizeof(buf), "/var/tmp/screen%d.pbb", x++);
			f = fopen(buf, "w");
		} while (f == NULL && errno == EEXIST);
	}

	if (f == NULL) {
		fprintf(logfd, "vesa open dump screen\n");
		return 0;
	}

	fprintf(f, "P6 %d %d 255\n", vesa_max_x, vesa_max_y);

	for (y=0; y<vesa_max_y; y++)
		for (x=0; x<vesa_max_x; x++, p++) {
			putc(p->r, f);
			putc(p->g, f);
			putc(p->b, f);
		}

	fclose(f);
	return 1;
}

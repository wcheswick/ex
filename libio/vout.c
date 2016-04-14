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
 * vout: VGA display output routines for control panel and video output.
 *
 * We use a modified version of libvgl.  The modifications include support
 * for multi-byte true colors, not palettes.  These routines only use these
 * full modes.
 */

This routine and output method is now deprecated. It always was just a 
bit outside what FreeBSD supported, and had the horrible property that
error messages would get written to a defunct screen.  It is left here
for reference, and might be useful as a template for future stuff.

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "vgl.h"

#include "util.h"
#include "io.h"
// #include "vesa.h"

static Pixel out_frame[MAX_Y][MAX_X];
Pixel (*video_out)[MAX_Y][MAX_X] = &out_frame;

Rectangle screen_rect;

char *prog;

int output_device = VGA_out;

int font_height;

void display(void) {
	refresh_screen = 1;
}

/*
 * Mouse is moving.  Mouse origin is the upper left corner, with
 * Y axis increasing down the screen.
 *
 * Motion is called when the mouse button is down
 */

void motion(int x, int y) {
#ifdef notdef
	int oy = FLIPY(y);
   
	screeny = display_height - (GLint) y;
	// fprintf(stderr, "%d %d   ", x, y);
	glFlush ();
#endif
}

/*
 * He's clicked the mouse. Button zero is on the left.
 */

enum mouse_state {
	Down = 0,
	Up = 1
};

/*
 * The mouse coordinates have the origin in the upper left.
 */
static void
do_click(int button, int state, int mousex, int mousey) {
	Point mouse = (Point){mousex,VESA_Y(mousey)};

	if (state == Down)
		click((Point){mouse.x, mouse.y});
}

void
set_screen_size(int w, int h) {
	screen_rect.min.x = 0;
	screen_rect.min.y = 0;
	screen_rect.max.x = screen_rect.min.x + w;
	screen_rect.max.y = screen_rect.min.y + h;
}

void
init_display(int argc, char *argv[], char *prog) {
	char *rm;
	int rc;

#ifdef OLD
	if (geteuid() != 0) {
		fprintf(stderr, "%s: VESA system calls need root access\n",
			argv[0]);
		exit(1);
	}
#endif

	rm = vesa_init(DX(screen_rect), DY(screen_rect));
	if (rm) {
		fprintf(stderr, "init_display (VESA): %s", rm);
		exit(1);
	}

	if (DX(screen_rect) > vesa_max_x || DY(screen_rect) > vesa_max_y) {
		vesa_end();
		fprintf(stderr, "init_display: doesn't fit on VESA VGA monitor\n");
		exit(1);
	}

	vesa_clear(Black);
	vesa_setmouse(0, 0, 0, Blue);

	if ((rc=VGLKeyboardInit(VGL_XLATEKEYS)) != 0) {
		fprintf(stderr, "VGLKeyboardInit error %d\n", rc);
		fprintf(logfd, "VGLKeyboardInit error %d\n", rc);
		vesa_end();
		exit(1);
	}
	//if ((rc=VGLMouseInit(VGL_MOUSESHOW)) != 0) {
	if ((rc=VGLMouseInit(VGL_MOUSEHIDE)) != 0) {
		fprintf(stderr, "VGLMouseInit error %d\n", rc);
		fprintf(logfd, "VGLMouseInit error %d\n", rc);
		vesa_end();
		exit(1);
	}
}

void
flush_screen(void) {
}

void
end_display(void) {
	VGLKeyboardEnd();
	vesa_end();
}

void
hide_cursor(void) {
	VGLMouseMode(VGL_MOUSEHIDE);	// This line will go away soon
	vesa_hide_cursor();
}

void
show_cursor(void) {
	// VGLMouseMode(VGL_MOUSESHOW);
	vesa_show_cursor();
}

void
draw_rect(Rectangle r, Pixel color) {
	vesa_draw_box(r, color);
}

void
fill_rect(Rectangle r, Pixel color) {
	vesa_fill_box(r, color);
}

/*
 * Define mouse cursor
 */
void
set_cursor(u_short *cursor) {
	vesa_setmouse(0, 0, 0, Blue);
}

static void
write_video_frame_zoom_2(Point p, Pixel (*frame)[MAX_Y][MAX_X]) {
	int x, y;
	int rc;

	/*
	 * We write a four pixel square for each video pixel,
	 * giving us a more-viewable 640x480 image.
	 */
	for (y=0; y<MAX_Y; y++) {
		Pixel *in_line = &(*frame)[y][0];
		Pixel line[MAX_X*2];
		int out_y = p.y+2*y;
		int out_x = p.x;

		for (x=0; x<MAX_X; x++)
			line[2*x] = line[2*x+1] = in_line[x];
		vesa_put_line((Point){out_x, out_y}, line, MAX_X*2);
		vesa_put_line((Point){out_x, out_y+1}, line, MAX_X*2);
	}
}

static void
write_video_frame_zoom_3(Point p, Pixel (*frame)[MAX_Y][MAX_X]) {
	int x, y;
	int rc;

	/*
	 * We write a four pixel square for each video pixel,
	 * giving us a more-viewable 640x480 image.
	 */
	for (y=0; y<MAX_Y; y++) {
		Pixel *in_line = &(*frame)[y][0];
		Pixel line[MAX_X*3];
		int out_y = p.y+3*y;
		int out_x = p.x;

		for (x=0; x<MAX_X; x++)
			line[3*x] = line[3*x+1] = line[3*x+2] = in_line[x];
		vesa_put_line((Point){out_x, out_y}, line, MAX_X*3);
		vesa_put_line((Point){out_x, out_y+1}, line, MAX_X*3);
		vesa_put_line((Point){out_x, out_y+2}, line, MAX_X*3);
	}
}

void
write_video_frame_zoom(Point p, Pixel (*frame)[MAX_Y][MAX_X], int zoom) {
	int x, y, i;
	int rc;

	/*
	 * We write a <zoom> pixel square for each video pixel,
	 * giving us a more-viewable <zoom>*(320x240) image.
	 *
	 * This is a main CPU hog, so we call optimizable routines for 
	 * the common cases.
	 */

	if (zoom == 2) {
		write_video_frame_zoom_2(p, frame);
		return;
#ifndef notdef
	/*
	 * this loop is 22.9/22.6 the speed of the more general code below.
	 * Why bother?  (Because it works?)
	 */
	} else if (zoom == 3) {
		write_video_frame_zoom_3(p, frame);
		return;
#endif
	}

	for (y=0; y<MAX_Y; y++) {
		Pixel *in_line = &(*frame)[y][0];
		Pixel line[MAX_X*zoom];
		int out_y = p.y+zoom*y;
		int out_x = p.x;

		for (x=0; x<MAX_X; x++) {
			for (i=0; i<zoom; i++)
				line[zoom*x + i] = in_line[x];
		}
		for (i=0; i<zoom; i++)
			vesa_put_line((Point){out_x, out_y+i}, line, MAX_X*zoom);
	}
}

void
write_row(Point p, int n_pixels, Pixel *row) {
	vesa_put_line((Point){p.x,p.y}, row, n_pixels);
}

int last_buttons = 0;

void
io_main_loop() {
	char key;
	int mouse_x, mouse_y;
	int buttons;

	while (1) {
		VGLCheckSwitch();
		if ((key = VGLKeyboardGetCh()) != 0) {
			kbd_hit(key);
			continue;
		}
		VGLMouseStatus(&mouse_x, &mouse_y, &buttons);
		vesa_mouseto((Point){mouse_x, mouse_y});
		if (buttons != last_buttons) {
			last_buttons = buttons;
			do_click(0, buttons != 0 ? Down : Up, mouse_x, mouse_y);
			continue;
		}
		do_idle();
	}
}

void
display_test(void) {
	Pixel32 *pp = (Pixel32 *)VGLDisplay->Bitmap;
	int x, y;

	for (y=0; y<32*4; y++) {
		int i = y/4;
		Pixel32 p;
		p.r = (i & 1) ? 0xff : 0;
		p.g = (i & 2) ? 0xff : 0;
		p.b = (i & 4) ? 0xff : 0;
		p.a = (i & 8) ? 0xff : 0;
		for (x=0; x<800; x++) {
			*pp++ = p;
		}
	}
}

/*
 * Write the current screen out to the given file in ppm format.
 *
 * This is not a critical routine, so errors aren't fatal.
 */
int
dump_screen(char *fn) {
	return vesa_dump_screen(fn);
}

void
write_pixmap(Point p, Rectangle r, PixMap pm) {
	int y;

	for (y=r.min.y; y<r.max.y; y++, p.y++)
		vesa_put_line(p, PIXMAPADDR(pm,0,y), r.max.x);
}

static int mouse_status;

void
screen_reserve(Rectangle r) {
	mouse_status = vesa_check_mouse(r);
}

void
screen_release(void) {
	vesa_restore_mouse(mouse_status);
}

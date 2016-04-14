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
 * xout: X output routines for control panel and video output.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef __FreeBSD__
#include <GL/glut.h>
#else
#include <GL/freeglut.h>
#endif
#include <GL/glx.h>

#include "libutil/util.h"
#include "io.h"

static image out_frame;
image *video_out = &out_frame;

char *prog;

int output_device = X_out;

Rectangle screen_rect;

int font_height;

static void
check_inscreen(Rectangle r) {
	assert(r.min.x >= 0 && r.max.x <= screen_rect.max.x &&
		r.min.y >= 0 && r.max.y <= screen_rect.max.y);
}

void
check_gl_error(char *routine) {
	GLenum errCode;

	if ((errCode = glGetError()) != GL_NO_ERROR) {
		fprintf(stderr, "GL error in %s, %s\n",
			routine, gluErrorString(errCode));
	}
}

void
display(void) {
fprintf(stderr, "display\n");
	glClear(GL_COLOR_BUFFER_BIT);
	check_gl_error("display 1");
	draw_screen();
	glFlush();
//	glutSwapBuffers();
	check_gl_error("display");
}

void
reshape(int w, int h) {
fprintf(stderr, "reshape\n");
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	check_gl_error("reshape 1");
	screen_rect.max.y = (GLint) h;
	glMatrixMode(GL_PROJECTION);
	check_gl_error("reshape 2");
	glLoadIdentity();
	check_gl_error("reshape 3");
	gluOrtho2D(0.0, (GLdouble) w, 0.0, (GLdouble) h);
	check_gl_error("reshape 4");
	glMatrixMode(GL_MODELVIEW);
	check_gl_error("reshape 5");
	glLoadIdentity();
	check_gl_error("reshape");
}

/*
 * Mouse is moving.  Mouse origin is the upper left corner, with
 * Y axis increasing down the screen.
 *
 * Motion is called when the mouse button is down
 */

void
motion(int x, int y) {
#ifdef notdef
	int oy = FLIPY(y);
   
	screeny = screen_rect.max.y - (GLint) y;
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
 * For some reason, the mouse coordinates have the origin in the
 * upper left!
 */
static void
do_click(int button, int state, int x, int mousey) {
	Point mouse = (Point){x,screen_rect.max.y - (GLint) mousey};

	if (state == Down)
		click((Point){mouse.x, mouse.y});
}

static void
keyboard(u_char key, int x, int y) {
	kbd_hit(key);
}

static char *screen_size = NULL;

void
set_screen_size(int w, int h) {
	char buf[100];
	char *screen_fmt = getenv("SCREEN_FMT");

	if (screen_fmt == 0)
		screen_fmt = "%dx%d";

	screen_rect.min.x = 0;
	screen_rect.min.y = 0;
	screen_rect.max.x = screen_rect.min.x + w;
	screen_rect.max.y = screen_rect.min.y + h;

	snprintf(buf, sizeof(buf), screen_fmt, DX(screen_rect), DY(screen_rect));
	screen_size = strdup(buf);
}

/*
 * Our origin is in the lower left, but for these GL routines, it
 * is in the upper left, going down!  screen_rect uses
 * our coordinates, not the GL coordinates.
 */
#define FONT	"-adobe-helvetica-bold-r-normal--20-140-100-100-p-105-iso8859-4"

static char reset_screen_cmd[100];

void
init_display(int argc, char *argv[], char *prog) {
	char buf[100];
	int ret;

	if (screen_size == 0) {
		fprintf(stderr, "init_display: screen size not set, aborting\n");
		exit(1);
	}

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

	// save the original screen size, and adjust the display to what
	// we want.  Shouldn't game mode do this?

	if (glutGet(GLUT_SCREEN_WIDTH) != DX(screen_rect) ||
	    glutGet(GLUT_SCREEN_HEIGHT) != DY(screen_rect)) {
		snprintf(reset_screen_cmd, sizeof(reset_screen_cmd),
			"xrandr -s %dx%d",
			glutGet(GLUT_SCREEN_WIDTH),
			glutGet(GLUT_SCREEN_HEIGHT));
		fprintf(stderr, "reset is %s\n", reset_screen_cmd);
		snprintf(buf, sizeof(buf), "xrandr -s %dx%d",
			DX(screen_rect), DY(screen_rect));
		ret = system(buf);
	}

	glutGameModeString(screen_size);
	if (glutGameModeGet(GLUT_GAME_MODE_POSSIBLE)) {
		glutEnterGameMode();
		fprintf(stderr, "init_display: screen set to %dx%d:%d@%d\n",
			glutGameModeGet(GLUT_GAME_MODE_WIDTH),
			glutGameModeGet(GLUT_GAME_MODE_HEIGHT),
			glutGameModeGet(GLUT_GAME_MODE_PIXEL_DEPTH),
			glutGameModeGet(GLUT_GAME_MODE_REFRESH_RATE));
	} else
		fprintf(stderr, "*** game mode not possible, display broken.\n");
	glClearColor (0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(do_click);
	glutMotionFunc(motion);
	glutIdleFunc(do_idle);
	check_gl_error("init_display 2");
}

void
end_display(void) {
	glutLeaveGameMode();
//	system(reset_screen_cmd);	// leave this for the calling script
}


#ifdef OLD	// to preserve the technology
Display *current;
Font glFont;

/*
 * Do the magic gl/X dance to get our hands on a font.
 */
void
set_font(int fs) {
        int listid;
	char *xfontname = 0;
        current = glXGetCurrentDisplay();
        listid = glGenLists(1);

#ifdef notdef
       XFontStruct *XLoadQueryFont(display, name)
             Display *display;
             char *name;
#ifdef notdef
helvB08-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--11-80-100-100-p-60-iso8859-4
helvB10-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--14-100-100-100-p-82-iso8859-4
helvB12-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--17-120-100-100-p-92-iso8859-4
helvB14-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--20-140-100-100-p-105-iso8859-4
helvB18-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--25-180-100-100-p-138-iso8859-4
helvB24-ISO8859-4.pcf.gz -adobe-helvetica-bold-r-normal--34-240-100-100-p-182-iso8859-4
#endif
#endif
	switch (fs) {
	case Small_font:
		xfontname = "-adobe-helvetica-bold-r-normal--11-80-100-100-p-60-iso8859-4";
		font_height = 11;	// XXX wrong
		break;
	case Medium_font:
		xfontname = "-adobe-helvetica-bold-r-normal--14-100-100-100-p-82-iso8859-4";
		font_height = 14;	// XXX wrong
		break;
	case Large_font:
		xfontname = "-adobe-helvetica-bold-r-normal--34-240-100-100-p-182-iso8859-4";
		font_height = 34;	// XXX wrong
		break;
	}

        glFont = XLoadFont(current, xfontname);
        glXUseXFont(glFont, 1, 127, listid);
	check_gl_error("set_font");
}
#endif

void
set_color(Pixel color) {
	glColor3f((float)color.r/(float)Z, (float)color.g/(float)(Z),
		(float)color.b/(float)Z);
	check_gl_error("set_color");
}

void
flush_screen(void) {
	glFlush();
//	glutSwapBuffers();	// GLUT_DOUBLE needs this
	check_gl_error("flush_screen");
}

void
printString(int x, int y, Pixel color, char *s) {
	set_color(color);
	if (debug)
		check_inscreen((Rectangle){{x, y}, {x+1, y+10}});
        glRasterPos2i(x, y);
	check_gl_error("printString 1");
        glPushAttrib(GL_LIST_BIT);
        glListBase(0);
	check_gl_error("printString 2");
	glCallLists(strlen(s), GL_UNSIGNED_BYTE, (GLubyte *) s);
	check_gl_error("printString 3");
        glPopAttrib();
	check_gl_error("printString");
}

void
io_main_loop() {
	glutMainLoop();
}

void
hide_cursor(void) {
}

void
show_cursor(void) {
}

/*
 * In our rectangles, max.x and max.y are one beyond the end of the rectangle,
 * a la Rob Pike.  The gl folks ignored rob, so we have to subtract one from
 * the max-s.
 */

void
fill_rect(Rectangle r, Pixel color) {
	set_color(color);
	if (debug)
		check_inscreen(r);

	glRecti(screen_rect.min.x + r.min.x, screen_rect.min.y + r.min.y,
		screen_rect.min.x + r.max.x,
		screen_rect.min.y + r.max.y);
	check_gl_error("fill_rect");
}

void
draw_rect(Rectangle r, Pixel color) {
	int x1 = screen_rect.min.x + r.min.x;
	int y1 = screen_rect.min.y + r.min.y;
	int x2 = screen_rect.min.x + r.max.x - 1;
	int y2 = screen_rect.min.y + r.max.y - 1;
	if (debug)
		check_inscreen((Rectangle){{x1,y1}, {x2,y2}});

	set_color(color);
	glBegin(GL_LINE_LOOP);
	glVertex2i(x1, y1);
	glVertex2i(x2, y1);
	glVertex2i(x2, y2);
	glVertex2i(x1, y2);
	glEnd();
	check_gl_error("draw_rect");
}

void
set_cursor(u_short *cursor) {
}

/*
 * These formats match the byte layouts for the 32-bit VGA screens on a PC.
 */
#define	PIXEL_TYPE	GL_UNSIGNED_INT_8_8_8_8_REV
#define PIXEL_FORMAT	GL_BGRA

void
write_video_frame_zoom(Point p, Pixel (*frame)[MAX_Y][MAX_X], int zoom) {
	int y;

	if (debug)
		check_inscreen((Rectangle){p, {p.x+MAX_X-1,p.y+1}});

	glPixelZoom (zoom, zoom);
	for (y=0; y<MAX_Y; y++) {
		Pixel *row = &((*frame)[y][0]);

		glRasterPos2i(p.x+0, p.y + (zoom*y));
		glDrawPixels(MAX_X, 1, PIXEL_FORMAT, PIXEL_TYPE, row);
	}
	glPixelZoom (1.0, 1.0);
	glFlush();
//	glutSwapBuffers();
}

void
write_row(Point p, int n, Pixel *row) {
	glRasterPos2i(p.x, p.y);
	check_gl_error("write_row 1");
	if (p.x < screen_rect.min.x || p.x + n > screen_rect.max.x ||
	    p.y < screen_rect.min.y || p.y >= screen_rect.max.y) {
		fprintf(stderr, "write_row off screen: %d-%d,%d in (%d,%d)x(%d,%d)\n",
			p.x, p.x+n, p.y,
			screen_rect.min.x, screen_rect.min.y,
			screen_rect.max.x, screen_rect.max.y);
		abort();
	}
	glDrawPixels(n, 1, PIXEL_FORMAT, PIXEL_TYPE, row);
//	glutSwapBuffers();
}

/*
 * The VGA routines have test patterns.  We don't use them here.
 */
void
display_test(void) {
}

/*
 * Write the current screen out to the given file in ppm format.
 *
 * This is not a critical routine, so errors aren't fatal.
 */
int
dump_screen(char *fn) {
	fprintf(stderr, "dump_screen not available for X window version.\n");
	return 0;
}

void
write_pixmap(Point p, Rectangle r, PixMap pm) {
	int y;

	if (debug)
		check_inscreen(r);

	for (y=0; y<DY(r); y++, p.y++)
		write_row(p, r.max.x, PIXMAPADDR(pm,0,y));
}

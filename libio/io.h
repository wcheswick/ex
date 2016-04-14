/*
 * Copyright 2004,2010 Bill Cheswick <ches@cheswick.com>
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
 * io.h - defines for the input and output interfaces for video and control.
 *	These routines are supplied by different files, one for each kind
 *	if supported input and output device.
 */

#define MAX_X	IN_W	// defined in the master Makefile, 640x360
#define MAX_Y	IN_H

//#define SCREEN_WIDTH	1024
//#define SCREEN_HEIGHT	768
#define SCREEN_WIDTH	1680
#define SCREEN_HEIGHT	1050

#define CENTER_X	(MAX_X/2)
#define CENTER_Y	(MAX_Y/2)

typedef Pixel image[MAX_Y][MAX_X];

#define	SETRGB(r,g,b)	(Pixel){b,g,r,Z}
#define	Z	0xff		/* brightest channel value */

#define	Black		SETRGB(0,0,0)
#define	Grey		SETRGB(Z/2,Z/2,Z/2)
#define	LightGrey	SETRGB(2*Z/3,2*Z/3,2*Z/3)
#define White		SETRGB(Z,Z,Z)

#define LightBlue	SETRGB(0,Z/2,Z)
#define DarkBlue	SETRGB(0,0,Z/2)
#define Blue		SETRGB(0,0,Z)

#define Red		SETRGB(Z,0,0)
#define LightRed	SETRGB(Z, Z/2, Z/2)
#define Green		SETRGB(0,Z,0)
#define LightGreen	SETRGB(Z/2,Z,Z/2)
#define DarkGreen	SETRGB(0,Z/2,0)
#define Magenta		SETRGB(Z,0,Z)
#define LightMagenta	SETRGB(Z,Z/2,Z)
#define Cyan		SETRGB(0,Z,Z)

#define Yellow		SETRGB(Z,Z,0)
#define Orange		SETRGB(Z/2,Z,Z)

#define LUM(p)	((((p).r)*299 + ((p).g)*587 + ((p).b)*114)/1000)

#define CLIP(c)	((c)<0 ? 0 : ((c)>Z ? Z : (c)))

/* Links to video input routines */

extern	image *video_in;

extern	int videoin_contrast;
extern	int videoin_hue;
extern	int videoin_sat;
extern	void grab_video_in(void);
extern	void init_video_in(void);

/*
 * Programs can vary their output processing based on the output
 * device, but this is discouraged---they are supposed to be the same,
 * with the X_out used for debugging.  Alas, there are some performance
 * issues that are getting in the way of other debugging.
 */
enum {
	X_out,
	VGA_out,
};

extern	int output_device;

/* Input video setting routines in ?in.c */

extern	void set_brightness(char brightness);
extern	void set_contrast(u_char contrast);
extern	void set_hue(char hue);
extern	void set_saturation(u_char sat);

extern	char get_brightness(void);
extern	u_char get_contrast(void);
extern	char get_hue(void);
extern	u_char get_saturation(void);

/* Callouts to the output display routines */

extern	void init_display(int argc, char *argv[], char *prog);
extern	void end_display(void);
extern	void set_screen_size(int w, int h);
extern	char *prog;

extern	void kbd_hit(char key);
extern	void io_main_loop(void);
extern	Point mouse;

extern	void flush_screen(void);
extern	void draw_screen(void);
extern	void draw_rect(Rectangle r, Pixel color);
extern	void fill_rect(Rectangle r, Pixel color);
extern	void write_row(Point p, int n, Pixel *row);

extern	void hide_cursor(void);
extern	void show_cursor(void);
extern	int string_len(char *s);
extern	void set_cursor(u_short *cursor);
extern	int dump_screen(char *fn);

extern	void paint_pixmap(Rectangle r, Pixel *pixmap);

extern	void write_video_frame_zoom(Point p, image *frame, int zoom);
extern	void write_pixmap(Point p, Rectangle r, PixMap pm);

extern	Pixel (*video_out)[MAX_Y][MAX_X];


/* in fileio.c */
extern	int write_jpeg_image(char *fn, image *frame);
extern	int read_jpeg_image(char *fn, image *, int flip);
extern	int decode_jpeg_image(FILE *jpegin, image *, int flip);
extern	Pixel *read_pnm_image(char *fn, int *dx, int *dy);

/* In the main program */

extern	void do_idle(void);
extern	void click(Point mouse);

extern	int refresh_screen;	// if we need to reinitialize the screen
extern	Rectangle screen_rect;

extern	void display_test(void);

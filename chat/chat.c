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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#include "arg.h"
#include "util.h"
#include "io.h"
#include "font.h"
#include "button.h"
#include "trans.h"

/*
 * In our program, the origin is always in the lower left.  This is certainly
 * not true for some or all of the I/O devices.
 */

/*
 * Video output display size.  NB:  Each input pixel is four output
 * pixels, to speed processing time.
 */

#define VIDEO_ZOOM	2

#define VIDEO_HEIGHT	(IN_H*VIDEO_ZOOM)
#define VIDEO_WIDTH	(IN_W*VIDEO_ZOOM)

#define BUTTON_WIDTH	(2*100)	/* pixels */
#define BUTTON_HEIGHT	(2*40)	/* pixels */
#define BUTTON_SEP	(2*8)
#define TOP_SPACE	10
#define TITLE_HEIGHT	50
#define TITLE_SEP	20

#define NSAMPLES	10	/* randomly-saved previous pictures */

#define LIVE_TO		(10)
#define ACTIVE_TO	(60)

#define DEMO_STRAIGHT_TO	4
#define DEMO_CHANGED_TO		12

#define ALARMTIME	15
#define WARNTIME	30

Rectangle video_r;
Rectangle title_r;
#ifdef FPS
Rectangle fps_r;
#endif

int grabtimeout = 4*60;
int idletime = 4*60;
int debug = 0;
int show_video_settings = 0;

font *title_font;
font *button_font;

long lastoptime = 0;

char *title;

#define FILENAMELEN	20

#define OLD	Green

button *last_command = 0;

char samplepics[NSAMPLES][FILENAMELEN];
short nsamples = 0;

int refresh_screen = 1;	/* if repaint (or initialize) the screen(s) */

Pixel frame[MAX_Y][MAX_X];
Pixel snapped_frame[MAX_Y][MAX_X];

enum {
	AttractLoop,
	Live,
	LiveTransform,
} state = Live;

typedef int b_func();
typedef void *b_init_func();

struct button_info {
	transform_t	*func;
	b_init_func	*init;
	void 	*call_param;
};

void *
bparam(transform_t func, b_init_func init) {
	struct button_info *bi = (struct button_info *)malloc(sizeof(struct button_info));
	assert(bi);

	bi->func = func;
	bi->init = init;
	bi->call_param = 0;
	return bi;
}

void
load_snapped_frame(void) {
	memcpy(&snapped_frame, video_in, sizeof(snapped_frame));
}

typedef	Pixel *bufptr[MAX_Y][MAX_X];

void
transform(button *bp, image in, image out) {
	struct button_info *bi = (struct button_info *)bp->param;
	b_func *func = bi->func;
	assert(func);	// button function processor missing
	(*func)(bi->call_param, in, out);
}

int
do_exit(void *param, image in, image out) {
	end_display();
	exit(0);
}

/*
 * Figure out where everything goes.
 */
void
layout_screen(void) {
	button *last_top;
	Rectangle r;
	Rectangle button_field_r;
	Rectangle exit_r = (Rectangle){{SCREEN_WIDTH-50, 0}, {SCREEN_WIDTH, 50}};
	title_font = load_font(TITLE_FONT);
	button_font = load_font(BUTTON_FONT);

	title = lookup("title", 0, "Chattanoooga Creative Discovery Museum Digital Darkroom");

	title_r.min.x = 0;
	title_r.min.y = SCREEN_HEIGHT - TOP_SPACE - TITLE_HEIGHT;
	title_r.max.x = SCREEN_WIDTH;
	title_r.max.y = SCREEN_HEIGHT - TOP_SPACE;

	video_r.min.x = (SCREEN_WIDTH-VIDEO_WIDTH)/2;
	video_r.min.y = title_r.min.y - TITLE_SEP - VIDEO_HEIGHT;
	video_r.max.x = video_r.min.x + VIDEO_WIDTH;
	video_r.max.y = video_r.min.y + VIDEO_HEIGHT;

#ifdef FPS
	fps_r.max.x = SCREEN_WIDTH;
	fps_r.min.x = fps_r.max.x - 130;
	fps_r.max.y = video_r.min.y - 3;
	fps_r.min.y = fps_r.max.y - FONTHEIGHT(title_font) - 2;
#endif

	button_field_r = (Rectangle){{(SCREEN_WIDTH - 5*(BUTTON_WIDTH+BUTTON_SEP))/2,0},
		{SCREEN_WIDTH, video_r.min.y-18}};

	r.min.x = button_field_r.min.x;
	r.max.x = r.min.x + BUTTON_WIDTH;
	r.max.y = button_field_r.max.y;
	r.min.y = r.max.y - BUTTON_HEIGHT;

	button_sep = BUTTON_SEP;
	set_font(button_font);

#ifdef notdef
	{"Seurat",	init_seurat, do_remap, "Seurat", "", 1, FRAME_COLOR},
	{"Picasso",	init_slicer, do_remap, "Picasso", "", 0, FRAME_COLOR},
	{"Cone projection", init_cone, do_remap, "Pinhead", "", 0, FRAME_COLOR},
	{"Dali",	init_dali, do_remap, "Dali", "", 1, AREA_COLOR},
	{"Fish eye",	init_fisheye, do_remap, "Fish", "eye", 0 ,AREA_COLOR},
	{"Munch",	init_twist, do_remap, "Edward", "Munch", 0, AREA_COLOR},
	{"Munch 2",	init_kentwist, do_remap, "Edward", "Munch 2", 0, AREA_COLOR}
#endif

	last_top = add_button(r, "matisse", "Matisse", Green, bparam(do_cfs,0));
	add_button(below(last_button->r), "monet", "Monet", Green, bparam(do_new_oil,0));
	last_top = add_button(right(last_top->r), "seurat", "Seurat", OLD,
		bparam(do_remap, init_seurat));
	add_button(below(last_button->r), "tintype", "Tin Type", Green, bparam(do_edge,0));

	last_top = add_button(right(last_top->r), "picasso", "Picasso", OLD,
		bparam(do_remap, init_slicer));
	add_button(below(last_button->r), "warhol", "Warhol", Green, bparam(cartoon, 0));

	last_top = add_button(right(last_top->r), "surreal", "Surreal", Green,
		bparam(do_neg_sobel, 0));
	add_button(below(last_button->r), "dali", "Dali", OLD, bparam(do_remap, init_dali));

	last_top = add_button(right(last_top->r), "munch1", "Edward|Munch 1", OLD,
		bparam(do_remap, init_twist));
	add_button(below(last_button->r), "munch2", "Edward|Munch 2", OLD,
		bparam(do_remap, init_kentwist));

#ifdef notdef
	add_button(right(last_top->r), "escher", "Escher", Green, do_remap);
	last_button->init = (void *)init_escher;
	last_top = last_button;
	add_button(below(last_button->r), "pinhead", "Pinhead", OLD, do_remap);
	last_button->init = (void *)init_cone;

	add_button(right(last_top->r), "neon", "Neon", Green, do_sobel);
	last_top = last_button;
	add_button(below(last_button->r), "fisheye", "Fish eye", OLD, do_remap);
	last_button->init = (void *)init_fisheye;

	add_button(right(last_top->r), "kite", "Kite", Green, do_remap);
	last_top = last_button;
	last_button->init = (void *)init_kite;
	add_button(below(last_button->r), "logo", "logo", Green, do_logo);

	add_button(right(last_top->r), "pixels", "Pixels", Green, do_remap);
	last_button->init = (void *)init_pixels4;

	add_button(exit_r, "exit", "(Exit)", Red, bparam(do_exit,0));
#endif
}

void
draw_screen(void) {
	button *bp = buttons;

	set_font(title_font);
	write_centered_string(title_r, White, title);

	while (bp) {
		paint_button(bp);
		bp = bp->next;
	}
	show_cursor();
	flush_screen();
}

#ifdef notyet

Pixel boredcolor = BOREDBACKGROUND;
void
bored_screen() {
	fill_rect(0, 0, 640, CONTROL_HEIGHT, boredcolor);
	write_string(textfont, 100, 200, White, "Please touch screen to begin");
}

void
show_demo_image(void) {
	int s = irand(nsamples);		/* pick the victim */

	if (nsamples == 0)
		return;
	reticle(LIVE);
	SetDispMode();	/* temporary grab */
	GetPic(samplepics[s], -1, -1, 0);
}

void
random_demo(void) {
	int i;
	char buf1[100], buf2[100];

	/*
	 * pick a command other than LIVE.
	 */
	i = irand(NLIST-1-1) + 1;
	if (list[i].type != 0)
		list[i].type(list[i].transform);
	else
		list[i].transform();
	i = sprintf(buf1, "  %s  ", list[i].name);
	snprintf(buf2, sizeof(buf2), "%*s", i, " ");
	write_string(textfont, 10, MAX_Y-8, White, buf1);
}
#endif

static int files_written = 0;

void
kbd_hit(char key) {
	char buf[100];

	switch (key) {
	case 'a':
		abort();
	case 'd':
		dump_screen(0);
		break;
	case 't':
		display_test();
		sleep(10);
		break;
	case 'w':
	case 'W':	/* write currently displayed image out to a file */
		snprintf(buf, sizeof(buf), "grab%03d.jpeg", files_written++);
		write_jpeg_image(buf, video_out);
		break;
	case 'q':
	case 'Q':
	case 'x':
	case 'X':	/* exit the program */
		end_display();
		exit(0);
	case 'v':
		show_video_settings = !show_video_settings;
		refresh_screen = 1;
		break;
	case 'h':
		set_hue(get_hue() - 1);
		refresh_screen = 1;
		break;
	case 'H':
		set_hue(get_hue() + 1);
		refresh_screen = 1;
		break;
	case 's':
		set_saturation(get_saturation() - 1);
		refresh_screen = 1;
		break;
	case 'S':
		set_saturation(get_saturation() + 1);
		refresh_screen = 1;
		break;
	case 'b':
		set_brightness(get_brightness() - 1);
		refresh_screen = 1;
		break;
	case 'B':
		set_brightness(get_brightness() + 1);
		refresh_screen = 1;
		break;
	case 'c':
		set_contrast(get_contrast() - 1);
		refresh_screen = 1;
		break;
	case 'C':
		set_contrast(get_contrast() + 1);
		refresh_screen = 1;
		break;
	}
}

#ifdef notyet
/*
 * Wait for someone to do something.  Return 1 if we time out waiting.
 */
int
touch_timeout(int time_limit, time_t start, int show_timeleft) {
	time_t timeleft;
	time_t lasttimeleft = 0;

	for (;;) {
#ifdef notyet
		if (mouse_pressed())
			break;
#endif
		timeleft = start + time_limit - time(0);
		if (timeleft <= 0) {
			fill_rect(0, WMINY, CONTROL_WIDTH, WMAXY, Black);
			return 1;
		}
		if (show_timeleft) {
			char buf[100];

			int lenx = ((CONTROL_WIDTH-BARX)*timeleft/time_limit);
			Pixel text_color = White;
			Pixel bar_color;

			if (timeleft == lasttimeleft)
				continue;

			if (timeleft > WARNTIME)
				bar_color = Green;
			else if (timeleft > ALARMTIME) {
				bar_color = Yellow;
				text_color = Black;
			} else
				bar_color = Red;
	
			snprintf(buf, sizeof(buf), " Seconds left: %ld", timeleft);
			fill_rect(0, WMINY, BARX+lenx, WMAXY, bar_color);
			fill_rect(BARX+lenx, WMINY, CONTROL_WIDTH,
				WMAXY, Black);
			write_string(textfont, 0, WMINY+7, text_color, buf);
			lasttimeleft = timeleft;
		}
	}
	fill_rect(0, WMINY, CONTROL_WIDTH, WMAXY, Black);
	return 0;
}

void
live_screen(void) {
	fill_rect(0, 0, CONTROL_WIDTH, CONTROL_HEIGHT, LIVEBACKGROUND);
	write_string(textfont, 100, 200, White, "Touch screen to take your picture");
}

void
do_grab(void) {
	int line, i;
	int samp = -1;

	reticle(LIVE);
	GrabFrame();
	SetDispMode();
	load_snapped_frame();
	if (nsamples < NSAMPLES)
		samp = nsamples++;
	else if (irand(10) < 2)
		samp = irand(NSAMPLES);
	if (samp >= 0)
		if (PutPic(samplepics[samp], 0, 0, MAX_X-1, MAX_Y-1, 0) < 0)
			fatal("Saving sample file");
}
#endif

void
process_command(Point mouse) {
	button *bp = buttons;

	while (bp && (bp->state == Hidden || !ptinrect(mouse, bp->r)))
		bp = bp->next;
	if (!bp)
		return;

	switch (state) {
	case AttractLoop:
		break;

	case Live:
		bp->state = On;
		paint_button(bp);
		state = LiveTransform;
		last_command = bp;
		break;
	case LiveTransform:
		last_command->state = Off;
		paint_button(last_command);
		if (bp == last_command) {
			state = Live;
		} else {
			bp->state = On;
			paint_button(bp);
		}
		last_command = bp;
		break;
	}
	flush_screen();
}

int mouse_pressed = 0;
Point last_mouse;

void
click(Point mouse) {
	mouse_pressed = 1;
	last_mouse = mouse;
	process_command(mouse);
}

#ifdef FPS
/*
 * Compute and display frames/second, at least for debugging.  We keep
 * a list of the last NFRAME display moments, and compute the display rate
 * over those NFRAME displays.
 */
#define NFRAME	10

long long times[NFRAME];
int next_time = 0;
int times_valid = 0;

void
show_fps(void) {
	struct timeval now;
	long long nowms;
	int ms;
	float fps;
	char buf[30];

	gettimeofday(&now, 0);
	nowms = now.tv_sec*1000 + now.tv_usec/1000;
	if (times_valid) {
		ms = nowms - times[next_time];
		fps = 1000.0/((float)ms/10.0);
		snprintf(buf, sizeof(buf), "frames/sec:   %4.1f", fps);
		set_font(title_font);
		write_string(fps_r, White, buf);
	}
	times[next_time] = nowms;
	next_time = (next_time + 1) % NFRAME;
	if (next_time == 0)
		times_valid = 1;
}
#endif

int
usage(void) {
	fprintf(stderr, "usage: chat [-h] [-d]\n");
	return 13;
}

void
init_transforms(void) {
	button *bp = buttons;

	while (bp) {	// how can a button not have a name? XXX
		struct button_info *bi = (struct button_info *)bp->param;
		b_init_func *init;

		init = bi->init;
		if (init)
			bi->call_param = (void *)(*init)();
		else
			bi->call_param = 0;
		bp = bp->next;
	}
}

char *prog;

int
main(int argc, char *argv[]) {
	prog = argv[0];

	load_config("chat");
	init_font_locale(lookup("locale", 0, 0));
	init_video_in();
	set_screen_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	srandom(time(0));
	layout_screen();
	init_polar();
	init_transforms();

	init_display(argc, argv, prog);

	ARGBEGIN {
	case 'd':	debug++;				break;
	case 'g':	grabtimeout = atoi(ARGF());		break;
	default:
		return usage();
	} ARGEND;

	if (grabtimeout != 0 && grabtimeout < ALARMTIME) {
		fprintf(stderr, "unreasonable grab time limit\n");
		exit(14);
	}

	io_main_loop();
	return 0;
}

void
do_idle(void) {
	if (refresh_screen) {
		draw_screen();
		refresh_screen = 0;
	}

	if (over_fps(30))
		return;

	switch (state) {
	case AttractLoop:
		break;

	case Live:
		grab_video_in();
		write_video_frame_zoom(video_r.min, video_in, VIDEO_ZOOM);
#ifdef FPS
		show_fps();
#endif
		break;

	case LiveTransform:
		grab_video_in();
		transform(last_command, *video_in, *video_out);
		write_video_frame_zoom(video_r.min, video_out, VIDEO_ZOOM);	
#ifdef FPS
		show_fps();
#endif
		flush_screen();
		break;
	}
}

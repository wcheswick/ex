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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#include "arg.h"
#include "io.h"
#include "chat.h"
#include "trans.h"

/*
 * In our program, the origin is always in the lower left.  This is certainly
 * not true for some or all of the I/O devices.
 */

/*
 * Video output display size.  NB:  Each input pixel is four output
 * pixels, to speed processing time.
 */
#define VIDEO_WIDTH	640
#define VIDEO_HEIGHT	480

/* Control panel layout */

#define CONTROL_WIDTH	640
#define CONTROL_HEIGHT	480

#define LEFTBUTTONSPACE	10
#define BUTTONBORDER	4

#ifdef notdef			// original chatenooga button size
#define TOPBUTTONSPACE	27
#define BUTTONWIDTH	200
#define BUTTONHEIGHT	100
#define BUTTONSEP	10
#else
#define TOPBUTTONSPACE	22
#define BUTTONWIDTH	200
#define BUTTONHEIGHT	60
#define BUTTONSEP	5
#endif

#define	FPSX		450		/* location of frames/second display */
#define FPSY		(CONTROL_HEIGHT - TOPBUTTONSPACE + 3)

/* Control panel colors */

#define CONTROLCOLOR	Red
#define POINT_COLOR	Green
#define AREA_COLOR	Blue
#define GEOM_COLOR	Magenta
#ifdef testing
#define FRAME_COLOR	Green
#endif
#define FRAME_COLOR	AREA_COLOR

#define NEW		Green

#define	BOREDBACKGROUND	Black
#define	LIVEBACKGROUND	Green
#define ACTIVEBACKGROUND Darkblue

#define NSAMPLES	10	/* randomly-saved previous pictures */

#define LIVE_TO		(10)
#define ACTIVE_TO	(60)

#define DEMO_STRAIGHT_TO	4
#define DEMO_CHANGED_TO		12

#define ALARMTIME	15
#define WARNTIME	30

int grabtimeout = 4*60;
int idletime = 4*60;
int debug = 0;

long lastoptime = 0;

#define FILENAMELEN	20

typedef struct commandlist	commandlist;

struct commandlist {
	char	*name;
	init_proc	*init;
	transform_t	*transform;
	char	*buttonlabel1, *buttonlabel2;
	int	column;
	Pixel	button_color;
	char	*description;
	Rectangle button;	/* where the button is */
	void 	*param;
} list[] = {
	{"snap/live",	0, 0, "snap a", "picture", 0, CONTROLCOLOR},
	{"Seurat?",	0, do_cfs, "Matisse", "", 0, NEW},
	{"Monet",	0, do_new_oil, "Monet", "", 0, NEW},
	{"Sobel",	0, do_sobel, "Neon", "", 0, NEW},
	{"logo",	0, do_logo, "Logo", "", 0,NEW},
	{"cartoon",	0, cartoon, "Warhol", "", 0,NEW},

	{"Seurat",	init_seurat, do_remap, "Seurat", "", 1, FRAME_COLOR},
	{"Picasso",	init_slicer, do_remap, "Picasso", "", 0, FRAME_COLOR},
	{"Cone projection", init_cone, do_remap, "Pinhead", "", 0, FRAME_COLOR},
	{"Neg",		0, do_neg_sobel, "Surreal", "", 0, NEW},
	{"edge",	0, do_edge, "Tin Type", "", 0, NEW},
	{"do_kite",	init_kite, do_remap, "do_kite", "", 0,NEW},

	{"Dali",	init_dali, do_remap, "Dali", "", 1, AREA_COLOR},
	{"Fish eye",	init_fisheye, do_remap, "Fish", "eye", 0 ,AREA_COLOR},
	{"Munch",	init_twist, do_remap, "Edward", "Munch", 0, AREA_COLOR},
	{"Munch 2",	init_kentwist, do_remap, "Edward", "Munch 2", 0, AREA_COLOR},
	{"Escher",	init_escher, do_remap, "Escher", "", 0, NEW},
	{"do_pixels4",	init_pixels4, do_remap, "do_pixels4", "", 0,NEW},

#ifdef notused
	{"Monet",	0, monet, "Monet", "", 0, NEW},
	{"Melt",	0, do_melt, "melt", "", 1, NEW},
#endif
	{0},
};

#define NLIST	(sizeof(list)/sizeof(struct commandlist))
#define LIVE_BUTTON	&list[0]

commandlist *lastcl = 0;

char samplepics[NSAMPLES][FILENAMELEN];
short nsamples = 0;

int refresh_screen = 1;	/* if repaint (or initialize) the screen(s) */

Pixel frame[MAX_Y][MAX_X];
Pixel snapped_frame[MAX_Y][MAX_X];

enum {
	AttractLoop,
	Snapped,
	Live,
	LiveTransform,
} state = Live;

void
fatal(char *message) {
	fprintf(stderr, "\n*** Fatal software error: %s\n", message);
	fprintf(stderr, "*** This should never happen:  please get help!\n");
	exit(13);
}

void
load_snapped_frame(void) {
	memcpy(&snapped_frame, video_in, sizeof(snapped_frame));
}

#ifdef notyet
/*
 * This creates the mask over the live scene, including an
 * X at dead center and little corner marks to delimit the
 * snapped_frame area.
 */
void
reticle(u_short value) {
	lineptr addr;
	u_short x, y;

	for (y=CENTER_Y-2; y<=CENTER_Y+2; y++) {
		addr = getlineptr(0, y);
		addr[CENTER_X] = value;
		if (y == CENTER_Y)
			for (x=CENTER_X-2; x<=CENTER_X+2; x++)
				addr[x] = value;
	}
	for (y=0; y<=5; y++) {
		addr = getlineptr(0, y);
		addr[0] = addr[MAX_X-1] = value; 		
		if (y == 0) {
			for (x=1; x<5; x++)
				addr[x] = addr[MAX_X-1-x] = value;
		}
	} 
	for (y=MAX_Y-1-5; y<MAX_Y; y++) {
		addr = getlineptr(0, y);
		addr[0] = addr[MAX_X-1] = value; 		
		if (y == MAX_Y-1) {
			for (x=1; x<5; x++)
				addr[x] = addr[MAX_X-1-x] = value;
		}
	} 
}
#endif

/*
 * control screen stuff
 */
void
put_text(char *s1, char *s2, Rectangle r) {
	int text_y;

	if (s2 == 0 || s2[0] == '\0') {
		control_write_string(r.min.x +
		    (BUTTONWIDTH - control_string_len(s1))/2,
		    r.min.y + BUTTONHEIGHT/2 - control_font_height/2, White, s1);
	} else {
		control_write_string(r.min.x +
		    (BUTTONWIDTH - control_string_len(s1))/2,
		    r.min.y + BUTTONHEIGHT/2, White, s1);
		control_write_string(r.min.x +
		    (BUTTONWIDTH - control_string_len(s2))/2,
		    r.min.y + BUTTONHEIGHT/2 - control_font_height, White, s2);
	}
}

void
set_button(commandlist *cl, int buttonstate) {
	Pixel color;

	control_hide_cursor();
	control_fill_rect(cl->button.min.x, cl->button.min.y,
		cl->button.max.x, cl->button.max.y, cl->button_color);

	if (buttonstate == Off) {
		color.r = cl->button_color.r/2;
		color.g = cl->button_color.g/2;
		color.b = cl->button_color.b/2;
	} else
		color = Black;
	control_fill_rect(cl->button.min.x+BUTTONBORDER,
		cl->button.min.y+BUTTONBORDER,
		cl->button.max.x-BUTTONBORDER, cl->button.max.y-BUTTONBORDER,
		color);

	if (cl == LIVE_BUTTON && buttonstate == On) {
		switch (state) {
		case Snapped:
			put_text("live", "image", cl->button);
			break;
		case LiveTransform:
			put_text("normal", "image", cl->button);
			break;
		default:
			abort();
		}
	} else
		put_text(cl->buttonlabel1, cl->buttonlabel2, cl->button);
	control_show_cursor();
	control_flush();
}

/*
 * The original control panel code made the VGA assumption that 0,0
 * was on the upper left.  We have to flip the Y.
 */
Rectangle
makerect(int column, int miny, int ncolumns) {
	Rectangle r;
	r.min.x = LEFTBUTTONSPACE + column*(BUTTONWIDTH + BUTTONSEP);
	r.min.y = miny;
	r.max.x = r.min.x + ncolumns*BUTTONWIDTH + (ncolumns-1)*BUTTONSEP;
	r.max.y = miny + BUTTONHEIGHT;
	return r;
}

typedef	Pixel *bufptr[MAX_Y][MAX_X];

void
transform(commandlist *ci, Pixel in[MAX_Y][MAX_X]) {
	ci->transform(ci->param, in, *video_out);
}

void
init_buttons(void) {
	commandlist *cl;
	short column;
	int row[10], i;

	for (i=0; i<10; i++)
		row[i] = 0;

	column = 0;
	for (cl = &list[0]; cl->name; cl++) {
		if (cl->column != 0) {
			column++;
			cl->column = column;
		} else
			cl->column = column;
		cl->button = makerect(cl->column, 
			CONTROL_HEIGHT - 1 - TOPBUTTONSPACE - (row[cl->column]+1)*(BUTTONHEIGHT + BUTTONSEP),
			1);
		row[cl->column]++;
	}
}

#define WMINX	0
#define WMINY	431
#define WMAXX	(WMINX+BUTTONWIDTH+BUTTONSEP+BUTTONWIDTH)
#define WMAXY	(WMINY+30)
#define	BARX	100

#ifdef notyet
void
writeframe(void) {
	static int writenum=0;
	u_char name[100], *cp = getenv("RAMDRIVE");
	sprintf(name, "%s\\saved%d", cp, writenum++);
	if (PutPic(name, 0, 0, MAX_X-1, MAX_Y-1, 0) < 0)
		perror("writeframe");
}
#endif

void
draw_control_screen() {
	commandlist *cl;

	control_hide_cursor();

	for (cl = &list[0]; cl->name; cl++)
		set_button(cl, Off);		// XXX this is wrong

	control_show_cursor();
	control_flush();
}

void
video_click(Point mouse) {
	/* Ignored for this program, we only click on the control panel. */
}

#ifdef notyet

void
init_files(void) {
	int i;
	char *cp = getenv("RAMDRIVE");
	char buf[100];

	if (cp == 0)
		fatal("RAMDRIVE not set\n");

	for (i=0; i<NSAMPLES; i++)
		sprintf(samplepics[i], "%s/s%02d", cp, i);
	sprintf(buf, "%s/test", cp);
	if (PutPic(buf, 0, 0, MAX_X-1, MAX_Y-1, 0) < 0)
		fatal("RAMDRIVE is not working");
}
#endif

Pixel boredcolor = BOREDBACKGROUND;
void
bored_screen() {
	control_hide_cursor();
	control_fill_rect(0, 0, 640, CONTROL_HEIGHT, boredcolor);
	control_write_string(100, 200, White, "Please touch screen to begin");
	control_show_cursor();
}

#ifdef notyet
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
	video_write_string(10, MAX_Y-8, White, buf1);
}
#endif

static int files_written = 0;

void
control_kbd_hit(char key) {
	char buf[100];

	switch (key) {
	case 'w':
	case 'W':	/* write currently displayed image out to a file */
		snprintf(buf, sizeof(buf), "grab%03d.jpeg", files_written++);
		write_image(buf, video_out);
		break;
	case 'q':
	case 'Q':
	case 's':
		
	case 't':
		display_test();
		break;
	case 'x':
	case 'X':	/* exit the program */
		end_video_in();
		end_video_out();
		end_control_panel();
		exit(0);
	}
}

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
			control_fill_rect(0, WMINY, CONTROL_WIDTH, WMAXY, Black);
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
			control_fill_rect(0, WMINY, BARX+lenx, WMAXY, bar_color);
			control_fill_rect(BARX+lenx, WMINY, CONTROL_WIDTH,
				WMAXY, Black);
			control_write_string(0, WMINY+7, text_color, buf);
			lasttimeleft = timeleft;
		}
	}
	control_fill_rect(0, WMINY, CONTROL_WIDTH, WMAXY, Black);
	return 0;
}

void
live_screen(void) {
	control_hide_cursor();
	control_fill_rect(0, 0, CONTROL_WIDTH, CONTROL_HEIGHT, LIVEBACKGROUND);
	control_write_string(100, 200, White, "Touch screen to take your picture");
	control_show_cursor();
}

#ifdef notyet
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

int
process_command(Point mouse) {
	commandlist *cl;

	for (cl = &list[0]; cl->name && !ptinrect(mouse, cl->button); cl++)
		;
	if (cl->name == 0)
		return 1;

	switch (state) {
	case AttractLoop:
		break;

	case Snapped:
		if (cl == LIVE_BUTTON) {
			if (lastcl)
				set_button(lastcl, Off);
			set_button(cl, Off);
			state = Live;
			lastcl = 0;
			break;
		}
		if (lastcl)
			set_button(lastcl, Off);
		set_button(cl, On);
		transform(cl, snapped_frame);
		video_write_frame(video_out);
		lastcl = cl;
		break;
	case Live:
		if (cl == LIVE_BUTTON) {
			state = Snapped;
			lastcl = 0;
			load_snapped_frame();
			video_write_frame(&snapped_frame);
			set_button(cl, On);
			break;
		}
		lastcl = lastcl = cl;
		set_button(cl, On);
		state = LiveTransform;
		set_button(LIVE_BUTTON, On);
		break;
	case LiveTransform:
		if (cl == LIVE_BUTTON) {
			if (lastcl)
				set_button(lastcl, Off);
			state = Live;
			set_button(cl, Off);
			lastcl = 0;
			break;
		}
		set_button(lastcl, Off);
		set_button(cl, On);
		lastcl = cl;
		break;
	}
	control_flush();
	return 1;
}

int mouse_pressed = 0;
Point last_mouse;

void
control_click(Point mouse) {
	mouse_pressed = 1;
	last_mouse = mouse;
	process_command(mouse);
}

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
		snprintf(buf, sizeof(buf), "frames/sec: %10.1f", fps);
		control_fill_rect(FPSX, FPSY, CONTROL_WIDTH,
			FPSY+control_font_height+1, Black);
		control_write_string(FPSX, FPSY, White, buf);
		control_flush();
	}
	times[next_time] = nowms;
	next_time = (next_time + 1) % NFRAME;
	if (next_time == 0)
		times_valid = 1;
}

int
usage(void) {
	fprintf(stderr, "usage: g [-c contrast] [-h hue] [-s saturation] [-i idletime]\n");
	return 13;
}

void
init_transforms(void) {
	commandlist *cl;
	for (cl = &list[0]; cl->name; cl++)
		if (cl->init)
			cl->param = (cl->init)();
		else
			cl->param = 0;
}

char *prog;

int
main(int argc, char *argv[]) {
	prog = argv[0];

	init_video_in();
	init_display(argc, argv, prog, CONTROL_WIDTH, CONTROL_HEIGHT+VIDEO_HEIGHT);

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

	srandom(time(0));
	init_buttons();
	init_video_out((Rectangle){{0, CONTROL_HEIGHT}, {VIDEO_WIDTH, VIDEO_HEIGHT}});
	init_control_screen(0, 0, CONTROL_WIDTH, CONTROL_HEIGHT);
	init_polar();
	init_transforms();
#ifdef notyet
	init_files();
#endif
	control_main_loop();
	return 0;
}

void
do_idle(void) {
	if (refresh_screen) {
		draw_control_screen();
		if (state == Snapped)
			video_write_frame(video_out);
		refresh_screen = 0;
	}

	switch (state) {
	case AttractLoop:
		break;

	case Snapped:
		break;

	case Live:
		grab_video_in();
		video_write_frame(video_in);
		show_fps();
		break;

	case LiveTransform:
		grab_video_in();
		transform(lastcl, *video_in);
		video_write_frame(video_out);	
		show_fps();
		control_flush();
		break;
	}
}

#ifdef oldloop
        for (state = Live; ; ) {
                time_t start;
                switch (state) {
                case Bored:
                        bored_screen();
                        for (;;) {
#ifdef notyet
                                show_demo_image();
#endif
                                if (!touch_timeout(DEMO_STRAIGHT_TO, time(NULL), 0))
                                        break;
                                random_demo();
                                if (!touch_timeout(DEMO_CHANGED_TO, time(NULL), 0))
                                        break;
                        }
                        state = Live;
                        /*FALLTHROUGH*/

                case Live:
                        do_live();
                        live_screen();
                        start = time(NULL);
                        if (touch_timeout(LIVE_TO, time(NULL), 0)) {
                                state = Bored;
                                break;
                        }
                        do_grab();
                        load_frame();
                        draw_control_screen();
                        start = time(NULL);
                        while (!touch_timeout(ACTIVE_TO, start, 1)) {
                                if (!process_command())
                                        break;
                                load_frame();
                        }
                        state = Live;
                }
        }
#endif

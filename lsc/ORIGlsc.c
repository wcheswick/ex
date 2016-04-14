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
#include "lsc.h"
#include "trans.h"

/*
 * In our program, the origin is always in the lower left.  This is certainly
 * not true for some or all of the I/O devices.
 */

/*
 * Video output display size.  NB:  Each input pixel is four output
 * pixels, to speed processing time.
 */
#define VIDEO_HEIGHT	480
#define VIDEO_WIDTH	640

/* Control panel layout */

#define CONTROL_WIDTH	640
#define CONTROL_HEIGHT	480

#define TOPBUTTONSPACE	27
#define LEFTBUTTONSPACE	0	// was 10

#define	FPSX		520		/* location of frames/second display */
#define FPSY		3 // was (CONTROL_HEIGHT - TOPBUTTONSPACE + 5)

/* Control panel colors */

int printcost = 0;
int grabtimeout = 4*60;
int idletime = 4*60;
int freebies = 0;
int printcount = 0;
int lastquartercount = 100;
int quartercount = 0;

time_t lastgrab = 0;

extern volatile int totalquarters;	/* incremented in cb.c */
extern volatile int quartercount;	/* incremented in cb.c */
extern volatile int cbfail;		/* coin box offline */


#define BUTTONBORDER	5	/* pixels */
#define BUTTONWIDTH	86	/* pixels */
#define BUTTONHEIGHT	48	/* pixels */
#define BUTTONSEP	6
#define TITLEHEIGHT	40

#define GRABWIDTH	(BUTTONWIDTH*2 + BUTTONSEP)

#define CONTROLCOLOR	Red
#define POINT_COLOR	Green
#define AREA_COLOR	Blue
#define GEOM_COLOR	Magenta
#define OTHER_COLOR	Cyan

#define MAXHIST		60
#define NSAMPLES	20	/* randomly-saved previous pictures */

#define ALARMTIME	15
#define WARNTIME	30


#define	BOREDBACKGROUND	Black
#define	LIVEBACKGROUND	Green
#define ACTIVEBACKGROUND Darkblue

#define LIVE_TO		(10)
#define ACTIVE_TO	(60)

#define DEMO_STRAIGHT_TO	4
#define DEMO_CHANGED_TO		12

#define ALARMTIME	15
#define WARNTIME	30

int debug = 0;

long lastoptime = 0;


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
#define LIVEBUTTON	0
#define GRABBUTTON	LIVEBUTTON
	{"Undo all",	0, 0, "undo all", "", 0, CONTROLCOLOR},
#define UNDOALLBUTTON	1
	{"Undo",	0, 0, "undo", "", 0, CONTROLCOLOR},
#define UNDOBUTTON	2
	{"Print",	0, 0, "print", "", 0, CONTROLCOLOR},
#define PRINTBUTTON	3
#define CONTROLBUTTONS	PRINTBUTTON

#define POINTCOL	0
	{"Luminance",		init_lum, do_point, "luminance", "", 0, POINT_COLOR},
	{"Brighten",		init_brighten, do_point, "brighter", "", 0, POINT_COLOR},
	{"Truncate brightness",	init_truncatepix, do_point, "truncate", "brightness", 0, POINT_COLOR},
	{"More Contrast",	init_high, do_point, "contrast", "", 0, POINT_COLOR},
	{"Op art",		0, op_art, "op art", "", 0, POINT_COLOR},
	{"Auto contrast", 	0, do_auto, "auto", "contrast", 0, POINT_COLOR},

	{"Negative", init_negative, do_point, "negative", "", 1, POINT_COLOR},
	{"Solarize", init_solarize, do_point, "solarize", "", 0, POINT_COLOR},
	{"Colorize", init_colorize, do_point, "colorize", "", 0, POINT_COLOR},
	{"Swap colors", init_swapcolors, do_point, "swap", "colors", 0, POINT_COLOR},
	{"Pixels",	init_pixels4, do_remap, "big", "pixels", 0, POINT_COLOR},
	{"Brownian pixels", 0, do_brownian, "Brownian", "pixels", 0, POINT_COLOR},

#define AREACOL		2
	{"Blur", 		0, do_blur, "blur", "", 1, AREA_COLOR},
	{"Bleed",		0, do_bleed, "bleed", "", 0, AREA_COLOR},
	{"Oil",			0, do_new_oil, "Oil", "paint", 0, AREA_COLOR},
	{"1 bit Floyd/Steinberg",	0, do_fs1, "Floyd/", "Steinberg", 0, AREA_COLOR},
	{"2 bit Floyd/Steinberg",	0, do_fs2, "2 bit", "F/S", 0, AREA_COLOR},
	{"color Floyd/Steinberg",	0, do_cfs, "color", "F/S", 0, AREA_COLOR},

	{"Focus", 		0, do_focus, "focus", "", 1, AREA_COLOR},
	{"Shear", 		0, do_shear, "shear", "", 0, AREA_COLOR},
	{"Sobel",		0, do_sobel, "Neon", "", 0, AREA_COLOR},
	{"Shower stall",	0, do_shower, "shower", "stall", 0, AREA_COLOR},
	{"Zoom",		init_zoom, do_remap, "zoom", "", 0, AREA_COLOR},
	{"Sampled zoom",	0, do_sampled_zoom, "sampled", "zoom", 0, AREA_COLOR},

#define GEOMCOL		4
	{"Mirror",		init_mirror, do_remap, "mirror", "", 1, GEOM_COLOR},
	{"Bignose",		init_fisheye, do_remap, "Fish", "eye", 0, GEOM_COLOR},
	{"Cylinder projection",	init_cylinder, do_remap, "cylinder", "", 0, GEOM_COLOR},
	{"Shower stall 2",	init_shower2, do_remap, "shower", "stall 2", 0, GEOM_COLOR},
	{"Cone projection",	init_cone, do_remap, "Pinhead", "", 0, GEOM_COLOR},
	{"Terry's kite",	init_kite, do_remap, "Terry's", "kite", 0, GEOM_COLOR},

	{"Rotate right",	init_rotate_right, do_remap, "rotate", "right", 1, GEOM_COLOR},
	{"Shift right",		init_shift_right, do_remap, "shift", "right", 0, GEOM_COLOR},
	{"Copy right", 		init_copy_right, do_remap, "copy", "right", 0, GEOM_COLOR},
	{"Twist right",		init_twist, do_remap, "Edward", "Munch", 0, GEOM_COLOR},
	{"Raise right",		init_raise_right, do_remap, "raise", "right", 0, GEOM_COLOR},
	{"Smear",		0, do_smear, "sine", "smear", 0, GEOM_COLOR},

	{"edge",		0, do_edge, "Edge", "Filter", 0, GEOM_COLOR},
// XXX	{"Difference",		0, do_diff, "difference", "last op", 0, GEOM_COLOR},
/*	{"Monet",		0, monet, "Monet", "(?)", 0, GEOM_COLOR},*/
// XXX	{"Seurat",		0, do_seurat, "Seurat", "", 0, GEOM_COLOR},
// XXX	{"Crazy Seurat",	0, do_crazy_seurat, "Crazy", "Seurat", 0, GEOM_COLOR},

#define OTHERCOL	6
	{"Logo",		0, do_logo, "logo", "", 1, OTHER_COLOR},
/*	{"Color logo",		0, do_color_logo, "color", "logo", 0, OTHER_COLOR},*/
	{"Spectrum",	 	0, do_spectrum, "spectrum", "", 0, OTHER_COLOR},
#ifdef test
	{"Can",			(proc)do_polar, (subproc)can, GS, "Can", "", 0, OTHER_COLOR},
	{"Pg",			(proc)do_polar, (subproc)pg, GS, "Pg", "", 0, OTHER_COLOR},
	{"Skrunch",		(proc)do_polar, (subproc)skrunch, GS, "Scrunch", "", 0, OTHER_COLOR},
	{"test", 0, cartoon, "Warhol", "", , 0, NEW},
	{"Seurat?", 0, do_cfs, "Matisse", "", 0, NEW},
	{"Slicer", 0, do_slicer, "Picasso", "", 0, OTHER_COLOR},
	{"Neg", 0, do_neg_sobel, "Surreal", "", 0, NEW},
	{"Dali", init_dali, do_remap, "Dali", "", 1, AREA_COLOR},
	{"Munch 2", init_kentwist, do_remap, "Edward", "Munch 2", 0, AREA_COLOR},
	{"Escher", init_escher, do_remap, "Escher", "", 0, NEW},
#endif

#ifdef notused
	{"Monet", 0, monet, "Monet", "", 0, NEW},
	{"Melt", 0, do_melt, "melt", "", 1, NEW},
#endif
	{0},
};

#define NLIST	(sizeof(list)/sizeof(struct commandlist))
#define LIVE_BUTTON	&list[0]

int refresh_screen = 1;	/* if repaint (or initialize) the screen(s) */

int nhist = 0;
struct history {
	image	frame;
	struct commandlist *command;
} history[MAXHIST];

int nsamples = 0;
image sample[NSAMPLES];


Pixel frame[MAX_Y][MAX_X];

enum {
	AttractLoop,
	Grabbed,
	Live,
	LiveTransform,
} state = Live;

void
fatal(char *message) {
	fprintf(stderr, "\n*** Fatal software error: %s\n", message);
	fprintf(stderr, "*** This should never happen:  please get help!\n");
	exit(13);
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
 * Draw text centered in a given rectangle
 */
void
put_text(char *s1, char *s2, Rectangle r) {
	int text_y;

	if (s2 == 0 || s2[0] == '\0') {
		control_write_string(r.min.x +
		    (r.max.x - r.min.x - control_string_len(s1))/2,
		    r.min.y + BUTTONHEIGHT/2 - control_font_height/2, White, s1);
	} else {
		control_write_string(r.min.x +
		    (r.max.x - r.min.x - control_string_len(s1))/2,
		    r.min.y + BUTTONHEIGHT/2, White, s1);
		control_write_string(r.min.x +
		    (r.max.x - r.min.x - control_string_len(s2))/2,
		    r.min.y + BUTTONHEIGHT/2 - control_font_height, White, s2);
	}
}

void
set_button(int ci, int buttonstate) {
	Pixel color;

	control_hide_cursor();
	control_fill_rect(list[ci].button.min.x, list[ci].button.min.y,
		list[ci].button.max.x, list[ci].button.max.y, list[ci].button_color);

	if (buttonstate == Off) {
		color.r = list[ci].button_color.r/2;
		color.g = list[ci].button_color.g/2;
		color.b = list[ci].button_color.b/2;
	} else
		color = Black;
	control_fill_rect(list[ci].button.min.x+BUTTONBORDER,
		list[ci].button.min.y+BUTTONBORDER,
		list[ci].button.max.x-BUTTONBORDER, list[ci].button.max.y-BUTTONBORDER,
		color);

#ifdef notneeded
	if (ci == LIVE_BUTTON && buttonstate == On) {
		switch (state) {
		case Grabbed:
			put_text("live", "image", list[ci].button);
			break;
		case LiveTransform:
			put_text("normal", "image", list[ci].button);
			break;
		default:
			abort();
		}
	} else
#endif
		put_text(list[ci].buttonlabel1, list[ci].buttonlabel2, list[ci].button);
	control_show_cursor();
	control_flush();
}

Rectangle
make_button_rect(int column, int maxy, int ncolumns) {
	Rectangle r;
	r.min.x = LEFTBUTTONSPACE + column*(BUTTONWIDTH + BUTTONSEP);
	r.min.y = maxy - BUTTONHEIGHT;
	r.max.x = r.min.x + BUTTONWIDTH*ncolumns;
	r.max.y = maxy;
	return r;
}

void
set_buttons(void) {
	int ci;
	short column;
	int row[10], i;

	for (i=0; i<10; i++)
		row[i] = 0;

	list[0].button.min.x = 0;
	list[0].button.max.x = list[0].button.min.x + GRABWIDTH; 
	list[1].button.min.x = list[0].button.max.x + BUTTONSEP;
	list[1].button.max.x = list[1].button.min.x + BUTTONWIDTH;
	list[2].button.min.x = list[1].button.max.x + BUTTONSEP;
	list[2].button.max.x = list[2].button.min.x + BUTTONWIDTH;
	list[3].button.min.x = list[2].button.max.x + BUTTONSEP;
	list[3].button.max.x = list[3].button.min.x + 2*BUTTONWIDTH;

	for (i=0; i<=CONTROLBUTTONS; i++) {
		/* place first four buttons across the top */
		list[i].button.min.y = CONTROL_HEIGHT - BUTTONHEIGHT;
		list[i].button.max.y = CONTROL_HEIGHT;
		set_button(i, Off);
	}

#define TITLE	(list[CONTROLBUTTONS].button.min.y-5)

#define TOPBUTTON	(TITLE-BUTTONSEP-BUTTONHEIGHT)

	column = POINTCOL;
	for (ci = CONTROLBUTTONS+1; list[ci].name; ci++) {
		if (list[ci].column != 0) {
			column++;
			list[ci].column = column;
		} else
			list[ci].column = column;
		list[ci].button = make_button_rect(list[ci].column, 
			TOPBUTTON - row[list[ci].column]*(BUTTONHEIGHT + BUTTONSEP),
			1);
		set_button(ci, Off);
		row[list[ci].column]++;
	}
}

u_char *
display_history(u_char *cp) {
	int n = nhist - 1;
	int nfullrows = n/2;
	int extra = n & 1;
	int r;

	cp += sprintf(cp, "\r\n\n");
	for (r=0; r<nfullrows; r++) {
		cp += sprintf(cp, "%2d  %-20s          ",
			r+1, history[r].command->name);
		cp += sprintf(cp, "%2d  %-20s\r\n",
			r+1+nfullrows+extra, history[r+nfullrows+extra].command->name);
	}
	if (extra)
		cp += sprintf(cp, "%2d  %-20s\r\n",
			nfullrows+extra,
			history[nfullrows+extra-1].command->name);
	return cp;
}

#define WMINX	0
#define WMINY	21
#define WMAXY	45
#define	BARX	130

void
clear_timer(void) {
	control_fill_rect(WMINX, WMINY, CONTROL_WIDTH, WMAXY, Black);
}

void
showtimer(void) {
	int timeleft = lastgrab + grabtimeout - time(0);
	int lenx;
	Pixel text_color = White;	// was brightwhite
	Pixel bar_color;
	char buf[50];

	if (grabtimeout == 0)
		return;
	lenx = ((CONTROL_WIDTH-BARX)*timeleft/grabtimeout);

	control_hide_cursor();

	control_fill_rect(0, WMINY, CONTROL_WIDTH, WMAXY, Black);

	if (timeleft > WARNTIME)
		bar_color = Green;
	else if (timeleft > ALARMTIME) {
		bar_color = Yellow;
		text_color = Black;
	} else
		bar_color = Red;

#ifdef notdef
	if (debug)
		snprintf(buf, sizeof(buf), " Seconds left: %d %.2X",
			timeleft, printer_status(CB));
	else
#endif
		snprintf(buf, sizeof(buf), " Seconds left: %d", timeleft);
	control_fill_rect(BARX, WMINY, BARX+lenx, WMAXY, bar_color);

	control_write_string(0, WMINY+7, text_color, buf);

	control_show_cursor();
}

void
do_last(void) {
	nhist = 1;
	video_write_frame(&history[0].frame);
}


u_short arrow[] = {
	/* screen mask */
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	/* cursor mask */
	0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
	0xd800, 0x9c00, 0x8c00, 0x0c00, 0x0600, 0x0600, 0x0300, 0x0300,
};

u_short coffee_cup[] = {
	/* screen mask */
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	/* coffee cup */
	0x0400, 0x0880, 0x1100, 0x0880, 0x0500, 0x0000, 0x0fc0, 0x1028,
	0x1434, 0x1224, 0x1228, 0x0830, 0x4fe4, 0x2008, 0x1ff0, 0x0000,
};

/*
 * Draw the current control screen
 */
void
draw_control_screen() {
	int ci;

	control_hide_cursor();

	for (ci=0; list[ci].name; ci++)
		set_button(ci, Off);	// XXX - this is wrong, and in chat, too

	put_text("Point", "Operations", make_button_rect(POINTCOL, TITLE, 2));
	put_text("Area", "Operations", make_button_rect(AREACOL, TITLE, 2));
	put_text("Geometric", "Operations", make_button_rect(GEOMCOL, TITLE, 2));
	put_text("Other", "Operations", make_button_rect(OTHERCOL, TITLE, 1));

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

void
transform(int ci, image in, image out) {
	list[ci].transform(list[ci].param, in, out);
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
	case 'x':
	case 'X':	/* exit the program */
		end_video_in();
		end_video_out();
		end_control_panel();
		exit(0);
	}
}

#ifdef notdef

void
check_kbd(void) {
	int ch = getch();
	switch (ch) {
	case 'x':
	case 'X':
		CloseMouse();
		if (printcost)
			coin_box_end();
		_setvideomode(_DEFAULTMODE);
		if ((printcost || printcount) && ch != 'x')
			print_summary(printcount, totalquarters, freebies);
		printf("Pages printed:            %4d\n",
			printcount);
		printf("Total quarters collected: %4d\n",
			totalquarters);
		printf("Free quarters given:      %4d\n",
			freebies);
		exit(0);
	case 't':
		time_a_quarter();
		break;
	case 's':
		show_spectrum();
		break;
	case 'q':
		quartercount++;
		freebies++;
		break;
	case 'c':
		cbfail = 0;
		break;
	case 'w':
		writeframe();
		break;
	case 'C':
		colorbars();
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		printcost = ch - '0';
		lastquartercount = 100;
		break;
	}
}
#endif

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

#ifdef notdef
void
do_demo(void) { 
	int i;
	char buf1[100], buf2[100];

	do {
		i = irand(NLIST-1);
	} while (list[i].button_color == CONTROLCOLOR);
	if (list[i].type != 0)
		list[i].type(list[i].transform);
	else
		list[i].transform();
	i = sprintf(buf1, "  %s  ", list[i].name);
	sprintf(buf2, "%*s", i, " ");
	fb_string(10, T_MAX_Y-8, buf1);
}
#endif

void
live_screen(void) {
	control_hide_cursor();
	control_fill_rect(0, 0, CONTROL_WIDTH, CONTROL_HEIGHT, LIVEBACKGROUND);
	control_write_string(100, 200, White, "Touch screen to take your picture");
	control_show_cursor();
}

int
lpt_print(void) {
	return 0;	// doesn't work
}

void
do_print(void) {
	if (quartercount < printcost)
		return;
	if (lpt_print()) {
		quartercount -= printcost;
		printcount++;
	}
}

#ifdef notyet

#define DELAY	3	/*seconds between samples*/
void
attractloop(void) {
	time_t timer = time(NULL);
	int moved = 0;
	int im, ix, iy;
	int m, x, y;

	get_mouse(&im, &ix, &iy);
	do {
		int i, elapsed;
		int s = irand(nsamples);		/* pick the victim */

		reticle(LIVE);
		SetDispMode();	/* temporary grab */
		GetPic(samplepics[s], -1, -1, 0);

		for (i=0; i<10; i++) {
			timer = time(NULL);
			do {
				elapsed = time(NULL) - timer;
				get_mouse(&m, &x, &y);
				moved = (m != im || x != ix || y != iy ||
					kbhit());
			} while (!moved && elapsed < DELAY);
			if (!moved)
				do_demo();
		}
	} while (!moved);

	cleartarga();
	reticle(~LIVE);
	SetOverMode();	/*back to live*/
}


attractloop(void) {
	time_t timer = time(NULL);
	int moved = 0;
	int im, ix, iy;
	int m, x, y;

	get_mouse(&im, &ix, &iy);
	do {
		int i, elapsed;
		int s = irand(nsamples);		/* pick the victim */

		reticle(LIVE);
		SetDispMode();	/* temporary grab */
		GetPic(samplepics[s], -1, -1, 0);

		for (i=0; i<10; i++) {
			timer = time(NULL);
			do {
				elapsed = time(NULL) - timer;
				get_mouse(&m, &x, &y);
				moved = (m != im || x != ix || y != iy ||
					kbhit());
			} while (!moved && elapsed < DELAY);
			if (!moved)
				do_demo();
		}
	} while (!moved);

	cleartarga();
	reticle(~LIVE);
	SetOverMode();	/*back to live*/
}

#endif

int laststatus = 0x100;

void
show_print_status(void) {
#ifdef notyet
	u_char buf[100];
	u_char *msg = buf;
	ushort prstat = printer_status(PR);

	if (cbfail)
		prstat = PR_CBFAIL;

	if (prstat == laststatus && quartercount == lastquartercount)
		return;

	laststatus = prstat;
	lastquartercount = quartercount;

	if (printcost)
		sprintf(list[PRINTBUTTON].buttonlabel1,
			"Print B&W picture.  Cost: $%04.2f",
			0.25*(float)printcost);
	else
		sprintf(list[PRINTBUTTON].buttonlabel1,	"Print B&W picture");
	switch (prstat) {
	case PR_CBFAIL:
		msg = "Printer unavailable: Coin box disconnected";
		break;
	case PR_NOPAPER:
		msg = "Printer unavailable: out of paper";
		coin_box(Off);
		break;
	case PR_OFFLINE:
		msg = "Printer unavailable: printer offline";
		coin_box(Off);
		break;
	case PR_OFF:
		msg = "Printer is turned off or not connected";
		coin_box(Off);
		break;
	case 0x10:	/* occurs during printing */
	case 0x90:
		if (quartercount < printcost) {
			if (!coin_box(On))
				msg = "Coin box disconnected";
			else if (quartercount + 1 == printcost) {
				if (printcost == 1)
					msg = "Insert a quarter";
				else
					msg = "Insert 1 more quarter";
			} else {
				if (quartercount == 0)
					sprintf(buf, "Insert %d quarters",
						printcost);
				else
					sprintf(buf, "Insert %d more quarters",
						printcost - quartercount);
			}
		} else {
			coin_box(Off);
			if (quartercount > printcost) /* shouldn't happen*/
				sprintf(buf,
					"Ready to Print.   Extra credit: $%04.2f",
					0.25*(float)(quartercount - printcost));
			else
				msg = "Ready to Print";
		}
		break;
	default:
		sprintf(buf, "Printer not available:  unexpected status %02x",
			prstat);
		if (printcost)
			coin_box(Off);
		break;
	}
	strcpy(list[PRINTBUTTON].buttonlabel2, msg);
	set_button(PRINTBUTTON, Off);
#endif
}

void
do_live(void) {
	list[LIVEBUTTON].buttonlabel1 = "Click anywhere to";
	list[LIVEBUTTON].buttonlabel2 = "take your picture";
	set_button(LIVEBUTTON, Off);
	clear_timer();
	state = Live;
}

void
do_grab(void) {
	int line, i;
	int samp = -1;

	grab_video_in();
	memmove(history[0].frame, *video_in, sizeof(frame));
	video_write_frame(&history[0].frame);
	nhist = 1;

	if (nsamples < NSAMPLES)
		samp = nsamples++;
	else if (irand(10) < 2)
		samp = irand(NSAMPLES);
	if (samp >= 0)
		memmove(sample[samp], history[0].frame, sizeof(frame));

	list[LIVEBUTTON].buttonlabel1 = "Camera on";
	list[LIVEBUTTON].buttonlabel2 = "";
	set_button(LIVEBUTTON, On);

	lastgrab = time(0);
	state = Grabbed;
}

int
process_command(Point mouse) {
	int ci;

	for (ci=0; list[ci].name; ci++)
		if (ptinrect(mouse, list[ci].button))
			break;
	if (list[ci].name == 0)
		return 1;

	switch (state) {
	case AttractLoop:
		break;

	case Grabbed:
		switch (ci) {
		case LIVEBUTTON:
			do_live();
			break;
		case UNDOALLBUTTON:
			nhist = 1;
			video_write_frame(&history[nhist-1].frame);
			break;
		case UNDOBUTTON:
			if (nhist > 1) {
				nhist--;
				video_write_frame(&history[nhist-1].frame);
			}
			break;
		case PRINTBUTTON:
			do_print();
			break;
		default:
			if (nhist == MAXHIST)
				break;
			transform(ci, history[nhist-1].frame, history[nhist].frame);
			nhist++;
			video_write_frame(&history[nhist-1].frame);
		}
		break;
	case Live:
		switch (ci) {
		case GRABBUTTON:
			do_grab();
			state = Grabbed;
			break;
		}
			
	case LiveTransform:
#ifdef notyet
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
#endif
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
		snprintf(buf, sizeof(buf), "%5.1f frames/sec.", fps);
		control_fill_rect(FPSX, FPSY, CONTROL_WIDTH,
			FPSY+control_font_height, Black);
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
	int ci;
	for (ci = 0; list[ci].name; ci++)
		if (list[ci].init)
			list[ci].param = (list[ci].init)();
		else
			list[ci].param = 0;
}

char *prog;

int
main(int argc, char *argv[]) {
	prog = argv[0];

	init_video_in();
	init_display(argc, argv, prog, CONTROL_WIDTH, CONTROL_HEIGHT+VIDEO_HEIGHT);

	ARGBEGIN {
	case 'd':	debug++;			break;
	case 'g':	grabtimeout = atoi(ARGF());	break;
	case 'i':	idletime = atoi(ARGF());	break;
	case '$':	printcost = atoi(ARGF());	break;
	default:
		return usage();
	} ARGEND;

	if (grabtimeout != 0 && grabtimeout < ALARMTIME) {
		fprintf(stderr, "unreasonable grab time limit\n");
		exit(14);
	}

	srandom(time(0));
	set_buttons();
	init_video_out((Rectangle){{0, CONTROL_HEIGHT}, {VIDEO_WIDTH, VIDEO_HEIGHT}});
	set_control_font(Medium_font);
	init_control_screen(0, 0, CONTROL_WIDTH, CONTROL_HEIGHT);
	init_polar();
	init_transforms();
	state = Live;
	control_main_loop();
	return 0;
}

time_t last_status_time = 0;

void
do_idle(void) {
	time_t now = time(0);

	if (refresh_screen) {
		draw_control_screen();
		if (state == Grabbed)
			video_write_frame(&history[nhist-1].frame);
		refresh_screen = 0;
	}

	show_print_status();

	switch (state) {
	case Live:
		// XXX test for attract loop time
		grab_video_in();
		video_write_frame(video_in);
		show_fps();
		break;

	case AttractLoop:
		break;

	case Grabbed:
		if (last_status_time != now) {
			showtimer();
			last_status_time = now;
			if (grabtimeout &&
			    lastgrab + grabtimeout <= now)
				do_live();
		}
		break;

	case LiveTransform:
#ifdef notyet
		grab_video_in();
		transform(lastcl, *video_in);
		video_write_frame(video_out);	
		show_fps();
		control_flush();
		break;
#endif
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

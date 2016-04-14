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
 * OF MERCHANTABFLITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABFLITY, WHETHER IN CONTRACT, STRICT LIABFLITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBFLITY OF SUCH DAMAGE.
 */

/*
 * This is a new version of the LSC software.  The differences from
 * the original:  the transforms are performed on live images, and
 * the screen contains both the video and and buttons.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include "arg.h"
#include "util.h"
#include "io.h"
#include "font.h"
#include "fancy_button.h"
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

#define VIDEO_HEIGHT	(MAX_Y*VIDEO_ZOOM)	// was (240*VIDEO_ZOOM)
#define VIDEO_WIDTH	(MAX_X*VIDEO_ZOOM)	// was (320*VIDEO_ZOOM)

//#define SCREEN_WIDTH	1280
//#define SCREEN_HEIGHT	1024
#define SCREEN_WIDTH	1680
#define SCREEN_HEIGHT	1050

int grabtimeout = 0; //4*60;
int idletime = 0; //4*60;

#define BUTTON_WIDTH	60	/* pixels */
#define BUTTON_HEIGHT	29	/* pixels */
#define BUTTON_SEP	15

#define MAXHIST		7
#define NSAMPLES	20	/* randomly-saved previous pictures */

#define ALARMTIME	15
#define WARNTIME	30

#define LIVE_TO		(10)
#define ACTIVE_TO	(60)

#define DEMO_STRAIGHT_TO	4
#define DEMO_CHANGED_TO		12

#define ALARMTIME	15
#define WARNTIME	30

int debug = 0;
int fps_flag = 0;	// If environment variable FPS set, show frames/second

long lastoptime = 0;

transform_t	do_exit;	// dummy transform

Rectangle video_r;	// where the video output goes
Rectangle fps_r;	// where we display frames per second
Rectangle hist_r;	// list of transforms we are performing
Rectangle explain_r;
Rectangle timer_r;	// The timeout section
Rectangle bar_r;	// The bar itself
Rectangle message_r;	// live/snapped message
Rectangle button_field_r;

font *text_font;
font *button_font;
font *big_button_font;

button *undo_button, *startover_button, *freeze_button;

button *current_primary_button;

enum {
	Cat_none = 0,
	Cat_change_color,
	Cat_change_look,
	Cat_change_shape,
};
#define NCATS	(Cat_change_shape+1)

Pixel catcolor[] = {
	Black,
	Green,
	LightBlue,
	Magenta,
	LightGreen,
	Orange,
	Yellow,
};


int refresh_screen = 1;		/* if repaint (or initialize) the screen */
int show_video_settings = 0;

/* We still call this history, for historical reasons. */

int nhist = 0;
button *history[MAXHIST];
image frames[MAXHIST+1];

int nsamples = 0;
image sample[NSAMPLES];

struct timeval last_busy;

Pixel frame[MAX_Y][MAX_X];

enum {
	AttractLoop,
	Live,
	Frozen,
} state = Live;

struct majorloc {
	button *b;
	int primary_bottom[NCATS];
	int secondary_bottom;
} primary_buttons[NCATS];

button *sample_secondary_button;

typedef int b_func();
typedef void *b_init_func();

struct button_info {
	transform_t	*func;
	b_init_func	*init;
	void 	*call_param;
	PixMap	name;
	PixMap	description;
};

/*
 * get the button bitmaps from files supplied by the LSC.
 */
static PixMap
get_pm(char *name, char *type, char *loc, int required) {
	static PixMap pm;
	int dx, dy;
	char fn[FILENAME_MAX];
	char dn[FILENAME_MAX];
	char *ffn;

	memset((void *)&pm, 0, sizeof(pm));

	if (type && type[0])
		snprintf(fn, sizeof(fn), "%s_%s.pnm", name, type);
	else
		snprintf(fn, sizeof(fn), "%s.pnm", name);
	snprintf(dn, sizeof(dn), "lib/lsc/%s", loc);
	ffn = find_file(fn, dn);
	if (ffn == 0) {
		if (!required)
			return pm;
		fprintf(stderr, "get_pm: could not find button image: %s, type %s, %s\n", name, type, fn);
		exit(10);
	}
	pm.pm = read_pnm_image(ffn, &dx, &dy);
	if (!pm.pm)
		exit(10);
	pm.r = (Rectangle){{0,0}, {dx,dy}};
	return pm;
}

button *
lsc_button(Point p, char *name, transform_t func, b_init_func init) {
	struct button_info *bi = (struct button_info *)malloc(sizeof(struct button_info));
	button *bp;
	PixMap up_pm = get_pm(name, "up", "buttons", 1);
	PixMap down_pm = get_pm(name, "dwn", "buttons", 1);

	if (p.y < 0)	// -y positions are distance from the top of the screen.
		p.y = SCREEN_HEIGHT - (-p.y);

	assert(bi);
	bi->func = func;
	bi->init = init;
	bi->call_param = 0;
	bi->name = get_pm(name, "", "names", 0);
#ifdef descriptions
	bi->description = get_pm(name, "", "descriptions", 0);
#endif
	bp = add_button_with_images(p, up_pm, down_pm, bi);
	bp->name = name;
	return bp;
}

/*
 * Figure out where everything goes.  The major buttons vary depending on which
 * category is selected.  We place the minor buttons now which always appear
 * in a given place, though they are obscured when their type isn't selected.
 */
void
layout_screen(void) {
	utf8 *touch_screen;
	Pixel color;
	PixMap pm_up, pm_down;
	int i;

	Rectangle r;
	Rectangle left_col, right_col, subcat_right, subcat_left;
	button *last_top;
	Point top_secondary;
	int last_secondary_bottom;

	text_font = load_font(TEXT_FONT);
	button_font = load_font(BUTTON_FONT);
	big_button_font = load_font(BIG_BUTTON_FONT);

	touch_screen = lookup("touchscreen", 0, "no");
	set_font(button_font);

	if (!set_background(find_file("background.pnm", "lib/lsc"), SCREEN_WIDTH, SCREEN_HEIGHT)) {
		fprintf(stderr, "lsc: background file %s missing\n", "background.pnm");
//		exit(1);
	}

//set_background(find_file("lsc_background.pnm", "buttons"), SCREEN_WIDTH, SCREEN_HEIGHT);

	video_r.min.x = 81;
	video_r.max.x = video_r.min.x + VIDEO_WIDTH;
	video_r.min.y = 229;
	video_r.max.y = video_r.min.y + VIDEO_HEIGHT;

	fps_r.max.x = video_r.max.x;
	fps_r.min.x = fps_r.max.x - 130;
	fps_r.max.y = video_r.min.y - 3;
	fps_r.min.y = fps_r.max.y - (FONTHEIGHT(text_font) + 5);

	message_r.max.y = fps_r.max.y;
	message_r.min.y = fps_r.min.y;
	message_r.min.x = video_r.min.x;
	message_r.max.x = fps_r.min.x;

// "Changes made" @ 875,-877; -899; -920; -940
	timer_r = (Rectangle){{0, 1080-1070}, {SCREEN_WIDTH, 45}};
	bar_r = timer_r;
	bar_r.min.x += 40;

	hist_r = (Rectangle){{575, 0}, {800, 160}};
	explain_r = (Rectangle){{16, SCREEN_HEIGHT-62},
		{SCREEN_WIDTH-307, SCREEN_HEIGHT-39}};

	freeze_button    = lsc_button((Point){900+230,109}, "freeze", 0, 0);
	undo_button	 = lsc_button((Point){900+50,109}, "undo", 0, 0);
	startover_button = lsc_button((Point){900+50,29}, "startover", 0, 0);
	current_primary_button = (button *)0;

	default_button_state = Off;
	default_category = Cat_none;

#ifdef notdef
	default_category = Cat_none;

	top_secondary = (Point){SECONDARY_X,top_y};
#endif

#define PRIMARY_X	(SCREEN_WIDTH-244)
#define SECONDARY_X	(SCREEN_WIDTH-184)
#define BUTTON_TOP	(SCREEN_HEIGHT - 32)

#define	BELOW	((Point){SECONDARY_X, (last_button->r.min.y - 60)})

	button_field_r = (Rectangle){{PRIMARY_X, 0},
		{SCREEN_WIDTH, BUTTON_TOP}};
	default_button_state = Off;

	/*
	 * Define the primary buttons.  We fill in the positions of
	 * the buttons depending on the primary selected.  This is uglier
	 * than it should be.
	 */
	default_button_state = Off;
	default_category = Cat_change_color;
	primary_buttons[Cat_change_color].b = lsc_button((Point){0,0}, "color", 0, 0);

	default_category = Cat_change_look;
	primary_buttons[Cat_change_look].b = lsc_button((Point){0,0}, "look", 0, 0);

	default_category = Cat_change_shape;
	primary_buttons[Cat_change_shape].b = lsc_button((Point){0,0}, "shape", 0, 0);

	default_button_state = Hidden;

	/* color changers */

	primary_buttons[Cat_change_color].primary_bottom[Cat_change_color] =
	    primary_buttons[Cat_change_color].primary_bottom[Cat_change_look] =
	    primary_buttons[Cat_change_color].primary_bottom[Cat_change_shape] =
		BUTTON_TOP - DY(primary_buttons[Cat_change_color].b->pm[On].r);

	default_category = Cat_change_color;
	primary_buttons[default_category].b->r.min.y = 
	    primary_buttons[default_category].primary_bottom[default_category];
	last_button = primary_buttons[default_category].b;

	lsc_button(BELOW, "blkwht", do_point, init_lum);
	sample_secondary_button = last_button;
	lsc_button(BELOW, "brghtr", do_point, init_brighten);
	lsc_button(BELOW, "dimmer", do_point, init_truncatepix);
	lsc_button(BELOW, "contrast", do_point, init_high);
	lsc_button(BELOW, "negative", do_point, init_negative);
	lsc_button(BELOW, "solar", do_point, init_solarize);
	lsc_button(BELOW, "colorize", do_point, init_colorize);
	lsc_button(BELOW, "outline", do_sobel, 0);
	lsc_button(BELOW, "raisedgray", do_edge, 0);
	primary_buttons[Cat_change_color].secondary_bottom =
		last_button->r.min.y - BUTTON_SEP;
	primary_buttons[Cat_change_look].primary_bottom[Cat_change_color] = 
	    primary_buttons[Cat_change_color].secondary_bottom -
	    DY(primary_buttons[Cat_change_look].b->pm[On].r);
	primary_buttons[Cat_change_shape].primary_bottom[Cat_change_color] = 
	    primary_buttons[Cat_change_look].primary_bottom[Cat_change_color] -
	    DY(primary_buttons[Cat_change_shape].b->pm[On].r) - BUTTON_SEP;


	/* look changers */

	primary_buttons[Cat_change_look].primary_bottom[Cat_change_look] =
	    primary_buttons[Cat_change_look].primary_bottom[Cat_change_shape] = 
	    	primary_buttons[Cat_change_color].primary_bottom[Cat_change_color] -
	    	DY(primary_buttons[Cat_change_look].b->pm[On].r) - BUTTON_SEP;;

	default_category = Cat_change_look;
	primary_buttons[default_category].b->r.min.y = 
	    primary_buttons[default_category].primary_bottom[default_category];
	last_button = primary_buttons[default_category].b;

	lsc_button(BELOW, "bigpixels", do_remap, init_pixels4);
	lsc_button(BELOW, "blur", do_blur, 0);
	lsc_button(BELOW, "blurry", do_brownian, 0);
	lsc_button(BELOW, "focus", do_focus, 0);
	lsc_button(BELOW, "bleed", do_bleed, 0);
	lsc_button(BELOW, "oilpaint", do_new_oil, 0);
	lsc_button(BELOW, "crackle", do_shower, 0);
	lsc_button(BELOW, "zoom", do_remap, init_zoom);
	lsc_button(BELOW, "earthqke", do_shear, 0);
	lsc_button(BELOW, "speckle", do_cfs, 0);
	primary_buttons[Cat_change_look].secondary_bottom = last_button->r.min.y - BUTTON_SEP;
	primary_buttons[Cat_change_shape].primary_bottom[Cat_change_look] = 
	    primary_buttons[Cat_change_look].secondary_bottom - 
	    DY(primary_buttons[Cat_change_shape].b->pm[On].r);

	/* shape changers */

	primary_buttons[Cat_change_shape].primary_bottom[Cat_change_shape] =
	    	primary_buttons[Cat_change_look].primary_bottom[Cat_change_shape] -
	    	DY(primary_buttons[Cat_change_shape].b->pm[On].r) - BUTTON_SEP;;
	default_category = Cat_change_shape;
	primary_buttons[default_category].b->r.min.y = 
	    primary_buttons[default_category].primary_bottom[default_category];
	last_button = primary_buttons[default_category].b;

	lsc_button(BELOW, "fisheye", do_remap, init_fisheye);
	lsc_button(BELOW, "flip", do_remap, init_mirror);
	lsc_button(BELOW, "mirror", do_remap, init_copy_right);
	lsc_button(BELOW, "kite", do_remap, init_kite);
	lsc_button(BELOW, "pinch", do_remap, init_cone);
	lsc_button(BELOW, "cylinder", do_remap, init_cylinder);
	lsc_button(BELOW, "wave", do_smear, 0);
	lsc_button(BELOW, "twist", do_remap, init_twist);
	lsc_button(BELOW, "escher", do_remap, init_escher);
	lsc_button(BELOW, "shower", do_remap, init_shower2);
	primary_buttons[Cat_change_shape].secondary_bottom = last_button->r.min.y - BUTTON_SEP;
}

void
clear_timer(void) {
	fill_rect(timer_r, Black);
}

#ifdef notdef
void
showtimer(void) {
	int timeleft = lastgrab + grabtimeout - time(0);
	int lenx;
	Pixel text_color = White;	// was brightwhite
	Pixel bar_color;
	Rectangle thisbar_r = bar_r;
	char buf[50];

	if (grabtimeout == 0)
		return;
	thisbar_r = bar_r;
	thisbar_r.max.x = bar_r.min.x +
		((bar_r.max.x - bar_r.min.x)*timeleft/grabtimeout);

	hide_cursor();

	clear_timer();

	if (timeleft > WARNTIME)
		bar_color = Green;
	else if (timeleft > ALARMTIME) {
		bar_color = Yellow;
	} else
		bar_color = Red;

	snprintf(buf, sizeof(buf), " Seconds left: %d", timeleft);
	fill_rect(thisbar_r, bar_color);

	write_string(timer_r.min, text_color, buf);

	show_cursor();
}
#endif

void
show_hist(void) {
	int i, y;
	int font_height = FONTHEIGHT(text_font);
	Point p = (Point){hist_r.min.x, hist_r.max.y};
	struct button_info *bi = 0;

	clear_to_background(hist_r);

	for (i=0; i<nhist; i++) {
		button *bp = history[i];

		bi = (struct button_info *)bp->param;
		p.y -= 2;
		p.y -= DY(bi->name.r);
		write_pixmap(p, bi->name.r, bi->name);
	}

#ifdef explain
	clear_to_background(explain_r);
	if (nhist) {
		if (DX(bi->description.r))
			write_pixmap(explain_r.min, bi->description.r, bi->description);
		else
			write_pixmap(explain_r.min, bi->name.r, bi->name);
	}
#endif
}

/*
 * Adjust the top-level button layouts for a new category.
 */
static void
set_category(button *b) {
	button *bp;
	int cat;
	int pb, sb, rb;

	clear_to_background(button_field_r);
	current_primary_button = b;

	for (bp = buttons; bp; bp = bp->next) {
		if (bp->category == Cat_none)
			continue;
		if (bp->category == current_primary_button->category)
			bp->state = Off;
		else
			bp->state = Hidden;
	}

	/*
	 * set the primary button positions for this primary selection.
	 */
	for (cat=Cat_change_color; cat <= Cat_change_shape; cat++) {
		button *b = primary_buttons[cat].b;
		b->r.min.x = PRIMARY_X;
		b->r.max.x = b->r.min.x + DX(b->pm[On].r);
		b->r.min.y =
		    primary_buttons[cat].primary_bottom[current_primary_button->category];
		b->r.max.y = b->r.min.y + DY(b->pm[On].r);
		b->state = Off;
		paint_fancy_button(b);
		flush_screen();
	}
	current_primary_button->state = On;
	cat = current_primary_button->category;

	for (bp = buttons; bp; bp = bp->next) {
		if (bp->category != cat)
			continue;
		paint_fancy_button(bp);
		flush_screen();
	}

	/* draw lines */
#define LSC_RED	(SETRGB(252,62,50))

	pb = primary_buttons[cat].primary_bottom[cat];
	sb = primary_buttons[cat].secondary_bottom + BUTTON_SEP;
	rb = PRIMARY_X+DX(current_primary_button->pm[On].r) - 2;

	fill_rect((Rectangle){{PRIMARY_X, sb+10}, {PRIMARY_X+2, pb}}, LSC_RED);
	fill_rect((Rectangle){{rb, sb+10}, {rb+2, pb}}, LSC_RED);
	fill_rect((Rectangle){{PRIMARY_X+10, sb}, {rb-10, sb+2}}, LSC_RED);

	/*
	 * Add a rounded corner to the lower left of the secondary area.  We
	 * Simply steal the lower 10x10 pixels of a secondary button, a kludge.
	 */
	write_pixmap((Point){PRIMARY_X, sb}, (Rectangle){{0,0}, {10,10}},
		sample_secondary_button->pm[Off]);
	flush_screen();
}

/*
 * Draw the current screen.
 */
void
draw_screen(void) {
	button *bp = buttons;

	clear_screen_to_background();
	while (bp) {
		if (bp->state != Hidden  && bp->category == Cat_none) {
			paint_fancy_button(bp);
			flush_screen();
		}
		bp = bp->next;
	}

	set_category(current_primary_button);
	show_hist();
	write_video_frame_zoom(video_r.min, &frames[nhist], VIDEO_ZOOM);
	show_cursor();
	flush_screen();
}

void
transform(button *bp, image in, image out) {
	struct button_info *bi = (struct button_info *)bp->param;
	b_func *func = bi->func;
	assert(func);	// button function processor missing
	(*func)(bi->call_param, in, out);
}

static int files_written = 0;

void
save_image(void) {
	char buf[100];
	struct stat sb;

	do {
		snprintf(buf, sizeof(buf), "/var/tmp/lsc%03d.jpeg", files_written++);
	} while (stat(buf, &sb) >= 0);
	write_jpeg_image(buf, &frames[nhist]);
	// fprintf(stderr, "wrote %s\n", buf);
}

int
do_exit(void *param, image in, image out) {
	end_display();
	exit(0);
}

int screen_dump_count = 0;

void
unfreeze(void) {
	if (state != Frozen)
		return;
	state = Live;
	freeze_button->state = Off;
	paint_fancy_button(freeze_button);
}

void
set_active(void) {
	gettimeofday(&last_busy, 0);
	if (state == AttractLoop)
		state = Live;
}

void
kbd_hit(char key) {
	set_active();
	switch (key) {
	case 'd':
		dump_screen(0);
		break;
	case 'w':
	case 'W':
		save_image();
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

int
process_command(Point mouse) {
	button *bp = buttons;

	while (bp && (bp->state == Hidden || !ptinrect(mouse, bp->r)))
		bp = bp->next;
	if (!bp)
		return 0;

	if (bp == undo_button) {
		if (nhist > 0) {
			nhist--;
			show_hist();
		}
	} else if (bp == startover_button) {
		unfreeze();
		nhist = 0;
		show_hist();
	} else if (bp == freeze_button) {
		switch (state) {
		case Live:
			state = Frozen;
			freeze_button->state = On;
			paint_fancy_button(freeze_button);
			break;
		case Frozen:
			unfreeze();
			break;
		default:
			break;
		}
		flush_screen();
	} else if (bp == primary_buttons[Cat_change_color].b ||
	   bp == primary_buttons[Cat_change_look].b ||
	   bp == primary_buttons[Cat_change_shape].b) {
		set_category(bp);
	} else {
		if (bp && nhist < MAXHIST) {
			history[nhist++] = bp;
			show_hist();
		}
	}
	flush_screen();
	return 1;
}

int mouse_pressed = 0;
Point last_mouse;

void
click(Point mouse) {
	set_active();
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
		set_font(big_button_font);
		write_string(fps_r, White, buf);
		flush_screen();
	}
	times[next_time] = nowms;
	next_time = (next_time + 1) % NFRAME;
	if (next_time == 0)
		times_valid = 1;
}

int
usage(void) {
	fprintf(stderr, "usage: lsc [-h] [-i idletime]\n");
	return 13;
}

void
init_transforms(void) {
	button *bp = buttons;

	while (bp && bp->name) {	// how can a button not have a name? XXX
		struct button_info *bi = (struct button_info *)bp->param;
		b_init_func *init = bi->init;
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

	fps_flag = (getenv("FPS") != NULL);

	load_config("lsc");
	init_font_locale(lookup("locale", 0, 0));
	srandom(0);
	set_screen_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	layout_screen();
	init_polar();
	init_transforms();
	init_video_in();
	init_display(argc, argv, prog);
	set_category(primary_buttons[Cat_change_color].b);
	gettimeofday(&last_busy, 0);

	ARGBEGIN {
	case 'd':	debug++;			break;
	case 'g':	grabtimeout = atoi(ARGF());	break;
	case 'i':	idletime = atoi(ARGF());	break;
	default:
		return usage();
	} ARGEND;

	if (grabtimeout != 0 && grabtimeout < ALARMTIME) {
		fprintf(stderr, "unreasonable grab time limit\n");
		exit(14);
	}

	state = Live;
	io_main_loop();
	return 0;
}

void
do_idle(void) {
	int i;
	struct timeval now;

	if (refresh_screen) {
		draw_screen();
		refresh_screen = 0;
		gettimeofday(&last_busy, 0);
	}

	switch (state) {
	case AttractLoop:
		if (over_fps(2))
			return;
		grab_video_in();
		write_video_frame_zoom(video_r.min, video_in, VIDEO_ZOOM);
		break;
	case Live:
		if (over_fps(30))
			return;
		grab_video_in();
		memcpy(frames[0], video_in, sizeof(frames[0]));
		/* FALLTHROUGH */
	case Frozen:
		for (i=0; i<nhist; i++)
			transform(history[i], frames[i], frames[i+1]);
		write_video_frame_zoom(video_r.min, &frames[nhist], VIDEO_ZOOM);
		gettimeofday(&now, 0);
		if (idletime > 0 && elapsed_time(last_busy, now) > idletime) {
			nhist = 0;
			unfreeze();
			draw_screen();
			state = AttractLoop;
		}
		if (fps_flag)
			show_fps();
		break;
	}
}

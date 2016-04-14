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
 * menu.c - select exhibits.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/limits.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <math.h>
#include <kvm.h>
#include <assert.h>

#include "arg.h"
#include "util.h"
#include "io.h"
#include "font.h"
#include "button.h"

#define SCREEN_WIDTH	800
#define SCREEN_HEIGHT	600

int debug = 0;
int refresh_screen = 1;

char **ex_list;

Rectangle title_r;
Rectangle subtitle_r;

font *text_font;
font *button_font;
font *title_font;
font *subtitle_font;

char *title;
char *subtitle;

#define BUTTON_SEP	10
#define BUTTON_WIDTH	(400)
#define BUTTON_HEIGHT	80
#define TITLE_SEP 	10
#define EXIT_BUTTON_MIN_X	(SCREEN_WIDTH - 50)	

struct exhibits {
	char *name;
	char *desc;
} exhibits[] = {
	{"chat", "Prototype|Chattanooga Children's Discovery Museum|Portrait Style Station"},
	{"lsc", "Prototype|Liberty Science Center|Digital Darkroom"},
	{"aud", "What does sound look like?"},
	{"lot", "How long does it take|to win the lottery?"},
	{"cb", "What do the colorblind see?"},
	{0,0},
};

int
do_exit(void) {
	end_display();
	exit(0);
}

int
execute(button *bp) {
	char buf[100];

	snprintf(buf, sizeof(buf), "exec run_exhibit %s", bp->name);
	end_display();
fprintf(stderr, "running '%s'\n", buf);
	system(buf);
	exit(0);	/* NOTREACHED */
}

/*
 * Figure out where everything goes.
 */
void
layout_screen(void) {
	int i, column;
	Point p;
	Rectangle r;
	button *last_row = 0;
	int maxbutt = 0;
	Rectangle exit_r = (Rectangle){{SCREEN_WIDTH-50, 0},
		{SCREEN_WIDTH, BUTTON_HEIGHT}};

	text_font = load_font(TEXT_FONT);
	button_font = load_font(BUTTON_FONT);
	title_font = load_font(TITLE_FONT);
	subtitle_font = load_font(SUBTITLE_FONT);
	title_r = (Rectangle){{0, SCREEN_HEIGHT - FONTHEIGHT(title_font)},
		{SCREEN_WIDTH, SCREEN_HEIGHT}};
	subtitle_r = (Rectangle){{0, title_r.min.y - 2*FONTHEIGHT(subtitle_font)},
		{SCREEN_WIDTH, title_r.min.y}};

	title = lookup("title", 0, "Select exhibit program");
	subtitle = lookup("subtitle", 0, "Choose one of the exhibit programs below");


	r.min.x = 0;
	r.max.x = r.min.x + BUTTON_WIDTH;
	r.max.y = subtitle_r.min.y - TITLE_SEP;
	r.min.y = r.max.y - BUTTON_HEIGHT;

	button_sep = BUTTON_SEP;
	set_font(button_font);

	for (i=0; exhibits[i].name; i++) {
		add_button(r, exhibits[i].name, exhibits[i].desc, Blue, (void *)i);
		r = below(last_button->r);
	}

	add_button(exit_r, "exit", "Exit", Red, (void *)-1);
}

void
kbd_hit(char key) {
	char buf[100];

	switch (key) {
	case 'a':
		abort();
	case 'd':
		dump_screen(0);
		break;
	case 'q':
	case 'Q':
	case 'x':
	case 'X':	/* exit the program */
		end_display();
		exit(0);
	}
}

/*
 * Draw the current screen.
 */
void
draw_screen(void) {
	button *bp = buttons;

	while (bp) {
		paint_button(bp);
		bp = bp->next;
	}

	set_font(title_font);
	write_centered_string(title_r, White, title);
	set_font(subtitle_font);
	write_centered_string(subtitle_r, White, subtitle);
	set_font(text_font);
	show_cursor();
	flush_screen();
}

Point last_mouse;

void
click(Point mouse) {
	button *bp = buttons;
	int func;

	while (bp && (bp->state == Hidden ||
	    bp->state == Unavailable ||
	    !ptinrect(mouse, bp->r)))
		bp = bp->next;
	if (!bp)
		return;

	func = (int)bp->param;
	if (func < 0)
		do_exit();
	execute(bp);
}

void
missclick_pause(void) {
	flush_screen();
	usleep(200*1000);	/* leave button on for 200 ms */
}

/*
 * Turn off unavailable exhibits.  Default is that all are on.  If there
 * is a list, turn off those not in the list.
 */
void
set_available(char **argv) {
	button *bp;
	char **list;

	if (*argv == 0)
		return;		// nothing, leave everything one

	for (bp=buttons; bp; bp = bp->next) {
		int ok = 0;
		for (list = argv; *list; list++) {
			if (strcmp(bp->name, *list) == 0) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			bp->state = Unavailable;
			paint_button(bp);
		}
	}
}

int
usage(void) {
	fprintf(stderr, "usage: menu [-d] [-h]\n");
	return 13;
}

char *prog;
int check_hardware = 0;

int
main(int argc, char *argv[]) {
	int x, y;
	prog = argv[0];

	load_config("menu");
	init_font_locale(lookup("locale", 0, 0));
	set_screen_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	layout_screen();

	init_display(argc, argv, prog);
	ARGBEGIN {
	case 'd':	debug++;			break;
	default:
		return usage();
	} ARGEND;

	set_available(argv);

	io_main_loop();
	return 0;
}

void
do_idle(void) {
	int i, n, len;

	if (refresh_screen) {
		draw_screen();
		refresh_screen = 0;
	}
}

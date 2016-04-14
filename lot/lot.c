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
 * lot.c - how long does it take to win the lottery?
 *
 * We implement the rules and average returns for the NJ Pick 6 lotto,
 * but are certainly not authorized to use their name.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

//#include <sys/limits.h> Ubuntu
//#include <sys/dkstat.h> Ubuntu
//#include <kvm.h> Ubuntu

#include "libio/arg.h"
#include "libutil/util.h"
#include "libio/io.h"
#include "libutil/font.h"
#include "libutil/button.h"

int debug = 0;

Rectangle title_r;
Rectangle subtitle_r;

Rectangle date_r;
Rectangle accounting_r;

font *text_font;
font *button_font;
font *title_font;
font *subtitle_font;

char *title;
char *subtitle;

#define BUTTON_SEP	10
#define BUTTON_WIDTH	40	// the area containing the input select buttons
#define BUTTON_HEIGHT	40
#define TITLE_SEP 	10
#define ACCT_R_MAX_Y	170
#define ACCT_SEP	260
#define PLAY_BUTTON_MAX_X	100
#define EXIT_BUTTON_MIN_X	(SCREEN_WIDTH - 50)	

#define GAMES_PER_LOOP	69013	// random number, should be odd

#define NBALLS		49	// for NJ Pick 6
#define BALLS_PICKED	6

#define BITS_PER_ULONG	(8*sizeof(u_long))
#define BALL_ULONG_SIZE	((NBALLS-1)/(BITS_PER_ULONG) + 1)

typedef int payout_list[BALLS_PICKED+1];

#define IS_BALL_SET(s,b) (s[(b-1)/BITS_PER_ULONG] & (1<<((b-1) % BITS_PER_ULONG)))
#define SET_BALL(s,b)	s[(b-1)/BITS_PER_ULONG] |= 1<<((b-1) % BITS_PER_ULONG)
#define CLEAR_SET(s)	memset(s, 0, BALL_ULONG_SIZE*(sizeof(u_long)/sizeof(u_char)))

u_long balls_selected_set[BALL_ULONG_SIZE];

int n_selected = 0;

int pick_count[BALLS_PICKED+1];
int pick_total[BALLS_PICKED+1];

int lotteries;
payout_list payout = {0, 0, 0, 3, 56, 2700, 13983816};

#define TICKET_COST	1

button *button_ptr[NBALLS+1];	/* runs from 1 to NBALLS */

button *play_button;
button *playonce_button;

int play_enabled = 0;

struct tm now;

int jdate;	// Julian date in current year.  0 = 1 Jan
int year;
int year_len;
int is_monday;

enum states {
	Attract,
	Selecting,
	Runonce,
	Running,
} state = Selecting;

#define DATE_HEIGHT		20
#define ACCOUNT_HEIGHT		200

/* Historical data from the NJ lottery pick 6 */
typedef struct lotdata lotdata;
struct lotdata {
	u_long winning_balls[BALL_ULONG_SIZE];
	int	big_winners;
	payout_list	payouts;
};

int nlotdata = 0;
lotdata *lot_history = 0;

int njackpots = 0;
int *jackpots = 0;



#define LOTDATA	"lotdata"
#define ALLOC_INCR	50

payout_list default_payout = {0, 0, 0, 3, 56, 2700, 16000000};

void
read_lot_history(void) {
	char *ffn;
	FILE *ld;
	int b[6];
	int nw;
	int i, n;
	int lot_alloced = 0;
	int jackpots_alloced = 0;
	payout_list pl = {0, 0, 0, 3, 56, 2700, 16000000};
	lotdata *dp;

	ffn = find_file(LOTDATA, "lib/lot");
	if (ffn == 0) {
		fprintf(stderr, "read_lot_history: could not find lottery history file: %s\n", LOTDATA);
		exit(10);
	}
	ld = fopen(ffn, "r");
	if (ld == 0) {
		fprintf(stderr, "lottery history data missing, using averages\n");
		nlotdata = 1;
		dp = lot_history = (lotdata *)malloc(sizeof(struct lotdata));
		assert(lot_history);	/* allocating lottery data */

		CLEAR_SET(lot_history->winning_balls);
		for (i=1; i<=6; i++)
			SET_BALL(dp->winning_balls, i);
		dp->big_winners = 1;
		memmove(dp->payouts, default_payout, sizeof(dp->payouts));
		return;
	}

	lot_alloced = 0;

	while ((n=fscanf(ld, "%d %d %d %d %d %d %d %d %d %d %d",
	    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
	    &nw, &pl[6], &pl[5], &pl[4], &pl[3])) == 11) {
		if (nlotdata == lot_alloced) {
			lot_alloced += ALLOC_INCR;
			lot_history =
				(lotdata *)realloc(lot_history,
				lot_alloced * sizeof(lotdata));
			assert(lot_history);
		}
		CLEAR_SET(lot_history[nlotdata].winning_balls);
		for (i=0; i<=5; i++)
			SET_BALL(lot_history[nlotdata].winning_balls, b[i]);
		memmove(lot_history[nlotdata].payouts, pl, sizeof(pl));
		nlotdata++;
		if (pl[6]) {
			if (njackpots == jackpots_alloced) {
				jackpots_alloced += ALLOC_INCR;
				jackpots = (int *)realloc(jackpots,
					jackpots_alloced*sizeof(int));
				assert(jackpots);
			}
			jackpots[njackpots++] = pl[6];
		}
	}
}

#define BI(p)	((void *)p)

#ifdef notdef
kvm_t kvm = 0;
#endif

void
kbd_hit(char key) {
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
 * display a large number with commas, and if selected, currency symbol.
 */
char *
bignum(int v, int currency) {
	static char buf[100];
	char *cur = currency ? "$" : "";	// XXX internationalization needed
	char *minus = (v < 0) ? "-" : " ";

	v = abs(v);

	if (v >= 1000000)
		snprintf(buf, sizeof(buf), "%s%s%3d,%03d,%03d", cur, minus, v/1000000,
			(v/1000) % 1000, v % 1000);
	else if (v > 1000)
		snprintf(buf, sizeof(buf), "%s%s%7d,%03d", cur, minus, 
			(v/1000) % 1000, v % 1000);
	else
		snprintf(buf, sizeof(buf), "%s%s%11d", cur, minus, v);
	return buf;
}

int
isleap(u_long year) {
	if (year % 4)
		return 0;
	if (year % 400)
		return 1;
	if (year % 100)
		return 0;
	return 1;
}

struct month_list {
	int	days;
	char	*name;
} months[12] = {
	{31, "Jan"},
	{28, "Feb"},
	{31, "Mar"},
	{30, "Apr"},
	{31, "May"},
	{30, "Jun"},
	{31, "Jul"},
	{31, "Aug"},
	{30, "Sep"},
	{31, "Oct"},
	{30, "Nov"},
	{31, "Dec"}
};

/*
 * Compute the date of the win.  We could do this easily with localtime
 * if time_r weren't Y2038 broken.
 */
void
show_date(int available) {
	char buf[100];
	char *dow;
	int m, days;
	int day = jdate;

	fill_rect(date_r, Black);
	if (!available)
		return;

	dow = is_monday ? "Mon" : "Thu";

	months[1].days = isleap(year) ? 29 : 28;
	for (m=0, days=0; day >= months[m].days; m++)
		day -= months[m].days;

	snprintf(buf, sizeof(buf), "%s %2d %s %d", dow, day+1, months[m].name, year);
	write_string(date_r, White, buf);
}

Rectangle
results(Rectangle r, char *text, int value, int currency) {
	char buf[100];

	snprintf(buf, sizeof(buf), "%s%12s", text, bignum(value, currency));
	if (value < 0)
		write_string(r, LightRed, buf);
	else
		write_string(r, White, buf);

	r.max.y = r.min.y;
	r.min.y = r.max.y - FONTHEIGHT(text_font);
	return r;
}

/*
 * write the results in the accounting area.
 */
void
display_results(void) {
	Rectangle r = (Rectangle){{accounting_r.min.x,
		accounting_r.max.y - FONTHEIGHT(text_font) - 1}, accounting_r.max};
	Rectangle full_r = r;
	int i, total = 0;

	r = results(r, "Games played", lotteries, 0);
	r = results(r, "   matched 3", pick_count[3], 0);
	r = results(r, "   matched 4", pick_count[4], 0);
	r = results(r, "   matched 5", pick_count[5], 0);
	r = results(r, "** matched 6", pick_count[6], 0);

	full_r.min.x += ACCT_SEP;
	r = full_r;

	total -= lotteries*TICKET_COST;
	r = results(r, "       ", -lotteries*TICKET_COST, 1);
	for (i=3; i<=BALLS_PICKED; i++) {
		total += pick_total[i];
		r = results(r, "       ", pick_total[i], 1);
	}
	r = results(r, " Total:", total, 1);

	full_r.min.x += ACCT_SEP;
	show_date(lotteries != 0);
}

/*
 * Draw the current screen.
 */
void
draw_screen(void) {
	button *bp = buttons;

fprintf(stderr, "draw_screen\n");
	while (bp) {
		paint_button(bp);
		bp = bp->next;
	}

	set_font(title_font);
	write_centered_string(title_r, White, title);
	set_font(subtitle_font);
	write_centered_string(subtitle_r, White, subtitle);
	set_font(text_font);
	display_results();
	show_cursor();
//	flush_screen();
}

void
clear_results(void) {
	int i;

	lotteries = 0;
	for (i=0; i<=BALLS_PICKED; i++)
		pick_total[i] = pick_count[i] = 0;
}

/*
 * Turn off the play button.
 */
void
disable_play(void) {
	if (!play_enabled)
		return;
	play_button->state = Unavailable;
	paint_button(play_button);
	playonce_button->state = Unavailable;
	paint_button(playonce_button);
	play_enabled = 0;
}

/*
 * Turn on the play button, and get ready to lotto!
 */
void
enable_play(void) {
	int i;

	if (play_enabled)
		return;
	play_button->state = Off;
	paint_button(play_button);
	playonce_button->state = Off;
	paint_button(playonce_button);
	play_enabled = 1;

	CLEAR_SET(balls_selected_set);
	for (i=1; i<=NBALLS; i++) {
		if (button_ptr[i]->state == On)
			SET_BALL(balls_selected_set,i);
	}
	clear_results();
	display_results();
}

void
next_lottery_date(void) {
	if (lotteries == 0) {
		time_t t = time(0);
		localtime_r(&t, &now);
		jdate = now.tm_yday;
		is_monday = 1;
		if (now.tm_wday <= 1)	// if Sun or Mon, jump to Monday's drawing
			jdate += 1 - now.tm_wday;
		else if (now.tm_wday <= 4) {	// if tue-thu, jump to thursday
			jdate += 4 - now.tm_wday;
			is_monday = 0;
		} else			
			jdate += 9 - now.tm_wday;	// jump to next Monday
		year = now.tm_year + 1900;
		year_len = isleap(year) ? 366 : 356;
	} else {
		jdate += is_monday ? 3 : 4;
		is_monday = 1- is_monday;
	}
	if (jdate >= year_len) {
		jdate -= year_len;
		year++;
		year_len = isleap(year) ? 366 : 365;
	}
}

Point last_mouse;

void
click(Point mouse) {
	int (*func)(button *bp, int v);
	button *bp = buttons;

	while (bp && (bp->state == Hidden || !ptinrect(mouse, bp->r)))
		bp = bp->next;
	if (!bp)
		return;

	assert(bp->param);	/* need function to execute */
	func = bp->param;
	(*func)(bp, bp->value);
}

void
missclick_pause(void) {
	flush_screen();
	usleep(200*1000);	/* leave button on for 200 ms */
}

int
do_select_ball(button *b) {
	if (state == Running)
		state = Selecting;
	if (b->state == Off) {
		b->state = On;
		paint_button(b);
		if (n_selected >= BALLS_PICKED) {
//			pause();
			missclick_pause();
			b->state = Off;
			paint_button(b);
			return 1;
		}
		n_selected++;
		if (n_selected == BALLS_PICKED)
			enable_play();
	} else {
		b->state = Off;
		paint_button(b);
		n_selected--;
		disable_play();
	}
	return 1;
}

int
do_exit(void) {
	end_display();
	exit(0);
}

int
do_play(button *b) {
	switch (b->state) {
	case On:
                state = Selecting;
                b->state = Off;
                paint_button(b);
                break;
	case Unavailable:
		break;
	case Hidden:
		break;
	case Off:
		b->state = On;
		paint_button(b);
		if (n_selected != BALLS_PICKED) {
			missclick_pause();
			b->state = Off;
			paint_button(b);
			break;
		}
		if (b == playonce_button)
			state = Runonce;
		else
			state = Running;
	}
	return 1;
}

int
random_ball(void) {
	return (random() % NBALLS) + 1;
}

/*
 * Pick six balls.  Since we are only picking 6 out of quite a few, it is
 * cheap just to try again if we have a duplicate.
 */

int *
pick_balls(void) {
	int n;
	u_long picked[2];
	static int pl[BALLS_PICKED];

	CLEAR_SET(picked);
	for (n=0; n<BALLS_PICKED; ) {
		int p = random_ball();
		if (IS_BALL_SET(picked,p))
			continue;	/* duplicate, try again */
		pl[n++] = p;
		SET_BALL(picked, p);
	}
	return &pl[0];
}

int
do_clear_selections(button *b, int v) {
	int i;

	if (state == Running)
		state = Selecting;
	b->state = On;		/* turn button on, for feedback */
	paint_button(b);

	for (i=1; i<=NBALLS && n_selected > 0; i++) {
		button *nb = button_ptr[i];
		if (nb->state == On) {
			nb->state = Off;
			paint_button(nb);
			n_selected--;
		}
	}
	clear_results();
	display_results();
	missclick_pause();
	b->state = Off;
	paint_button(b);
	clear_results();
	disable_play();
	return 1;
}

int
do_choose_balls(button *b, int v) {
	int *list = pick_balls();
	int i;

	do_clear_selections(b, v);

	for (i=0; i<BALLS_PICKED; i++) {
		button *pb = button_ptr[list[i]];
		pb->state = On;
		paint_button(pb);
	}
	n_selected = BALLS_PICKED;
	enable_play();
	return 1;
}

int
one_game(void) {
	int i;
	int *list = pick_balls();
	int matched = 0;

	next_lottery_date();
	lotteries++;

	for (i=0; i<BALLS_PICKED; i++)
		if (IS_BALL_SET(balls_selected_set, list[i]))
			matched++;

	pick_count[matched]++;

	/*
	 * The lot_history contains the failed jackpot data, i.e. zeros,
	 * which we have already taken care of but running through the loop.
	 * so we have a separate array of payouts for the jackpots.
	 */
	if (matched == BALLS_PICKED)
		pick_total[matched] += jackpots[random() % njackpots];
	else
		pick_total[matched] +=
			lot_history[random() % nlotdata].payouts[matched];
	return (matched == BALLS_PICKED);
}

/*
 * Compute a bunch of lottery results and then rest a moment to update
 * display, check the mouse, etc.
 */
void
lotto(int once) {
	int i;

	for (i=0; i<GAMES_PER_LOOP; i++)
		if (one_game() || once) {
			state = Selecting;
			play_button->state = Off;
			paint_button(play_button);
			playonce_button->state = Off;
			paint_button(playonce_button);
			break;
		}
	display_results();
}

/*
 * Figure out where everything goes.
 */
void
layout_screen(void) {
	int i, column;
	Rectangle r;
	button *last_row = 0;
	int maxbutt = 0;

	text_font = load_font(TF);
	button_font = load_font(BF);
	title_font = load_font(TIF);
	subtitle_font = load_font(STIF);
	title_r = (Rectangle){{0, SCREEN_HEIGHT - FONTHEIGHT(title_font)},
		{SCREEN_WIDTH, SCREEN_HEIGHT}};
	subtitle_r = (Rectangle){{0, title_r.min.y - 3*FONTHEIGHT(subtitle_font)},
		{SCREEN_WIDTH, title_r.min.y}};

	date_r = (Rectangle){{200, 0},
		{SCREEN_WIDTH-50, FONTHEIGHT(text_font)}};

	title = lookup("title", 0, "How long does it take to win the lottery?");
	subtitle = lookup("subtitle", 0, "Choose six numbers and play $1 twice a week automatically until|you win!  You are also paid for matching three, four, or five numbers.");


	r.min.x = 0;
	r.max.x = r.min.x + BUTTON_WIDTH;
	r.max.y = subtitle_r.min.y - TITLE_SEP;
	r.min.y = r.max.y - BUTTON_HEIGHT;

	button_sep = BUTTON_SEP;
	set_font(button_font);

	for (column=0, i=1; i <= NBALLS; i++) {
		char buf[10];

		snprintf(buf, sizeof(buf), "%d", i);
fprintf(stderr, "%s", buf);
		add_button(r, buf, buf, Blue, BI(do_select_ball));
fprintf(stderr, "	%p", last_button);
		last_button->value = i;
		button_ptr[i] = last_button;
		if (column == 0)
			last_row = last_button;
		column++;
fprintf(stderr, "	%d\n", column);
		if (column == 10) {
			if (last_button->r.max.x > maxbutt)
				maxbutt = last_button->r.max.x;
			r = below(last_row->r);
			column = 0;
		} else
			r = right(last_button->r);
	}

	r.min.x = maxbutt + BUTTON_SEP;
	r.max.x = SCREEN_WIDTH;
	r.max.y = button_ptr[1]->r.max.y;
	r.min.y = r.max.y - (BUTTON_HEIGHT*1.5);
 	add_button(r, "clear", "Clear|selections", Green, BI(do_clear_selections));

	add_button(below(last_button->r), "compchoose", "Computer|chooses",
		Green, BI(do_choose_balls));

	play_button = add_button(below(last_button->r), "play", "Play!",
		Red, BI(do_play));
	play_button->state = Unavailable;

	playonce_button = add_button(below(last_button->r), "once", "Play once",
		Red, BI(do_play));
	playonce_button->state = Unavailable;

	accounting_r = (Rectangle){{0, 0},
		{play_button->r.min.x, button_ptr[NBALLS]->r.min.y - TITLE_SEP}};
}

int
usage(void) {
	fprintf(stderr, "usage: lot [-d] [-h]\n");
	return 13;
}

#ifndef srandomdev

#define STATE_SIZE	256

void
srandomdev(void) {
	char state[STATE_SIZE];
	int n, rfd;

	initstate(0, state, STATE_SIZE);
	rfd = open("/dev/urandom", O_RDONLY);
	if (rfd < 0) {
		perror("opening /dev/urandom");
		exit(1);
	}
	n = read(rfd, state, sizeof(state));
	if (n != sizeof(state)) {
		perror("read /dev/urandom error, continuing");
	}
}

#endif

char *prog;
int check_hardware = 0;

int
main(int argc, char *argv[]) {
	prog = argv[0];

	load_config("lot");
	init_font_locale(lookup("locale", 0, 0));
	srandomdev();
	read_lot_history();
	set_screen_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	layout_screen();

	init_display(argc, argv, prog);
	ARGBEGIN {
	case 'd':	debug++;			break;
	case 'h':	check_hardware++;		break;
	default:
		return usage();
	} ARGEND;

	io_main_loop();
	return 0;
}

void
do_idle(void) {
	switch (state) {
	case Attract:
		break;
	case Selecting:
		break;
	case Runonce:
		lotto(1);
		state = Selecting;
		break;
	case Running:	
		lotto(0);
		break;
	}
}

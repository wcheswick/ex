#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <curses.h>

#include "elo.h"

#define TOUCH_SCREEN	"/dev/cuad0"

double A, B, C, D;

int
get_touch(int *x, int *y) {
	int rx, ry, rz, stat;
	int sum_x=0, sum_y=0;
	int n = 0;

	do {
		while (!is_elo_data())
			sleep(0);
		if (!get_elo_touch(&rx, &ry, &rz, &stat))
			continue;	// should never happen

		sum_x += rx;
		sum_y += ry;
		n++;

	} while (!(stat & 0x04));

	if (n == 0)
		return 0;

	*x = sum_x/n;
	*y = sum_y/n;
	return 1;
}

#ifdef notdef
void
cal(double x0, double y0, double x1, double y1,
    double c0, double r0, double c1, double r1,
    double *a, double *b, double *c, double *d) {
	*a = (y0*r1 - r0*y1) / (x0*y1 - x1*y0);
	*b = (r1 - *a*x1)/y1;
	*c = (y0*c1 - c0*y1) / (x0*y1 - x1*y0);
	*d = (c1 - *a*x1)/y1;
}

void
calibrate(void) {
	int ll_x, ll_y;
	int ul_x, ul_y;
	int ur_x, ur_y;
	int lr_x, lr_y;

	fprintf(stderr, "Touch lower left hand corner of screen\n");
	get_touch(&ll_x, &ll_y);

	fprintf(stderr, "Touch upper left hand corner of screen\n");
	get_touch(&ul_x, &ul_y);

	fprintf(stderr, "Touch upper right hand corner of screen\n");
	get_touch(&ur_x, &ur_y);

	fprintf(stderr, "Touch lower right hand corner of screen\n");
	get_touch(&lr_x, &lr_y);

	fprintf(stderr, "%d %d  %d %d  %d %d  %d %d\n",
		ll_x, ll_y, ul_x, ul_y, ur_x, ur_y, lr_x, lr_y);

	cal(ul_x, ul_y, lr_x, lr_y, 0, 1023, 1279, 0, &A, &B, &C, &D);
	fprintf(stderr, "%9.6f %9.6f %9.6f %9.6f\n", A, B, C, D);

//	cal(ll_x, ll_y, ur_x, ur_y, 0, 1023, 1279, 0, &a, &b, &c, &d);
//	fprintf(stderr, "%9.6f %9.6f %9.6f %9.6f\n", a, b, c, d);
}
#endif

struct {
	int x, y;
} cals[3];

void
cross(int pos) {

	switch(pos) {
	case 0:
		setsyx(0,0);
		printw(" |\n");
		printw("-+-\n");
		printw(" |\n");
		break;
	case 1:
		setsyx(22,0);
		printw(" |\n");
		printw("-+-\n");
		printw(" |\n");
		break;
	case 2:
		setsyx(0,77);
		printw(" |");
		setsyx(1,77);
		printw("-+-");
		setsyx(2,77);
		printw(" |");
		break;
	default:
		abort();
	}

	setsyx(12, 35);
	printw("%d Touch the marker\n", pos);
	refresh();
}

void
calibrate(void) {
	int i;

	initscr();

	for (i=0; i<3; i++) {
		cross(i);
		get_touch(&cals[i].x, &cals[i].y);
		setsyx(5, 5);
		printw("got %d %d\n", cals[i].x, cals[i].y);
	}

	endwin();
}

int
main(int argc, char *argv[]) {
	int x, y;
	u_char r[ELO_PACKET_SIZE];

	if (setup_elo_tty(TOUCH_SCREEN) < 0)
		return 1;

	if (init_elo() < 0)
		return 2;

	setbuf(stdout, NULL);

	elo_calibrate_and_scale(817, 3861, 752, 3557, 0, 1279, 0, 1023);

	while (1) {
		get_touch(&x, &y);
		printf("%d %d\n", x, y);
	}

	return 0;
}

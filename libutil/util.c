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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/time.h>

#include "libutil/util.h"
#include "libio/io.h"
#include "font.h"

struct config_line {
	char *label;
	utf8 *value;
	struct config_line *next;
};

int have_background = 0;
PixMap background;

int background_width;
int background_height;

struct config_line *config_list = 0;

char line[2000];

/*
 * Return the current wall clock time, in milliseconds.
 */
l_time_t
rtime(void) {
	struct timeval tv;
	gettimeofday(&tv, 0);

	return (l_time_t)tv.tv_sec*1000 + tv.tv_usec/1000;
}

int
ptinrect(Point p, Rectangle r) {
	return p.x >= r.min.x && p.x < r.max.x &&
		p.y >= r.min.y && p.y < r.max.y;
}

/*
 * Is there any overlap between the rectangles?
 */
int
rectinrect(Rectangle r1, Rectangle r2) {
	if (r1.max.y <= r2.min.y || r1.min.y >= r2.max.y ||
	    r1.max.x <= r2.min.x || r1.min.x >= r2.max.x)
		return 0;
	else
		return 1;
}

Rectangle
inset(Rectangle r, int i) {
	return (Rectangle){{r.min.x+i, r.min.y+i}, {r.max.x-i, r.max.y-i}};
}

/*
 * Create and initialize a PixMap
 */
void
make_PixMap(PixMap *pm, Rectangle r, Pixel color) {
	int i, n = DX(r) * DY(r);
	Pixel *pp;

fprintf(stderr, "make_PixMap: n=%d,   r=%d,%d x %d,%d  pm=%p\n",
	n, r.min.x, r.min.y, r.max.x, r.max.y, pm);

fprintf(stderr, " a ");
	pp = (Pixel *)malloc(n * sizeof(Pixel));
fprintf(stderr, " b ");
	assert(pp);	/* allocating PixMap */
fprintf(stderr, " c\n");
	pm->pm = pp;
	pm->r = r;
	for (i=0; i<n; i++)
		*pp++ = color;
}

/*
 * Read a line.  Skip blank ones.
 */
int
get_nonempty_line(FILE *f) {
	char *cp;

	do {
		char *lp = fgets(line, sizeof(line), f);
		if (lp == NULL)
			return 0;

	} while (line[0] == '\n');

	if ((cp=strchr(line, '\n')) != 0)
		*cp = '\0';
	return 1;
}

/*
 * Return a pointer to a string, either the value in our list of configuration pairs,
 * or the default string supplied by the caller.  If the second string is non-null,
 * append it to the first string before looking up the label.  If the appended version
 * isn't found, return a match to the first name, if found, else the default.
 */
utf8 *
lookup(char *name, char *name2, utf8 *default_label) {
	char full_name[100];
	struct config_line *cl = config_list;
	utf8 *value = 0;

	if (name2) {
		snprintf(full_name, sizeof(full_name), "%s%s", name, name2);
		name2 = full_name;
	}

	while (cl) {
		if (name2) {
			if (strcmp(name2, cl->label) == 0)  // if exact match
				return cl->value;
		}
		if (strcmp(name, cl->label) == 0) {
			if (name2 == 0)
				return cl->value;
			else
				value = cl->value;
		}
		cl = cl->next;
	}
	if (value)
		return value;
	else
		return default_label;
}

/*
 * The config file contains a list of tab-separated name/value pairs.
 * The name is a text label, and the value is a string, ending at EOL.
 * This is used to set values for strings and format strings that may
 * vary from site to site, espcially for foreign language support.
 *
 * We store these values in a simple linked list, and search them linearly.
 * They will probably only be used during initialization.
 */
static int
load_config_file(char *fn) {
	struct stat sb;
	struct config_line *last = 0;
	struct config_line *cl;
	FILE *f = NULL;
	char *cp = 0;

	if (stat(fn, &sb) != 0)
		return 0;

	f = fopen(fn, "r");
	if (f == NULL)
		return 0;

	fprintf(stderr, "Configuring from %s ...\n", fn);

	while (get_nonempty_line(f)) {
		if (!isalnum(line[0]))
			continue;

		if ((cp = strchr(line, '\t')) == 0)
			continue;
		*cp++ = '\0';

		for (cl=config_list; cl; cl = cl->next) {
			if (strcmp(cl->label, line) == 0)
				break;
			last = cl;
		}
		if (cl) {	/* overwrite previous value */
			free(cl->value);
			cl->value = strdup(cp);
			break;
		}

		cl = (struct config_line *)malloc(sizeof(struct config_line));
		assert(cl);	// allocating configuration entries
		cl->label = strdup(line);
		cl->value = strdup(cp);
		cl->next = 0;
		assert(cl->label && cl->value); //allocating configuration information

		if (config_list == 0)
			config_list = cl;
		else
			last->next = cl;
		last = cl;
	}
	fclose(f);
	return 1;
}

/*
 * We cast around for config files in several directories.  In each directory
 * where config file 'fn' is found, load the data.  Later entries override
 * earlier ones, so the default values should be in the first ones.  It is intended
 * that nearly all text, as well as default values for various things, be
 * settable by these entries.  The entries have UTF-8 encodings, which I hope
 * will handle most internationalization issues.
 */
void
load_config(char *fn) {
	char buf[500];
	char *dir;
	int nfiles = 0;

	/* These next entries make debugging a little easier */
	snprintf(buf, sizeof(buf), "lib/conf/%s", fn);
	nfiles += load_config_file(buf);

	snprintf(buf, sizeof(buf), "../lib/conf/%s", fn);
	nfiles += load_config_file(buf);

#ifdef CONFIGDIR
	snprintf(buf, sizeof(buf), "%s/%s", CONFIGDIR, fn);
	nfiles += load_config_file(buf);
#endif

	snprintf(buf, sizeof(buf), "/usr/local/lib/ex/conf/%s", fn);
	nfiles += load_config_file(buf);

	dir = getenv("CONFIG");
	if (dir) {
		snprintf(buf, sizeof(buf), "%s/%s", dir, fn);
		nfiles += load_config_file(buf);
	}
}

char *
find_file(char *fn, char *dir) {
	static char buf[500];
	struct stat sb;

	snprintf(buf, sizeof(buf), "/home/ches/nex/%s/%s", dir, fn);
	if (debug > 1) fprintf(stderr, "find_file trying %s\n", buf);
	if (stat(buf, &sb) == 0)
		return buf;

	snprintf(buf, sizeof(buf), "/home/ches/nex/%s", fn);
	if (debug > 1) fprintf(stderr, "find_file trying %s\n", buf);
	if (stat(buf, &sb) == 0)
		return buf;

	snprintf(buf, sizeof(buf), "/home/ches/nex/lib/%s", fn);
	if (debug > 1) fprintf(stderr, "find_file trying %s\n", buf);
	if (stat(buf, &sb) == 0)
		return buf;

	if (debug)
		fprintf(stderr, "find_file: could not locate '%s' in '%s'\n",
			fn, dir);
	return 0;
}

long long
dt_ms(struct timeval now, struct timeval before) {
	long long now_ms, before_ms;
	now_ms = now.tv_sec*1000LL + now.tv_usec/1000;
	before_ms = before.tv_sec*1000LL + before.tv_usec/1000;
	assert(now_ms >= before_ms);
	return now_ms - before_ms;
}

struct timeval last_display_time = {0,0};

int
over_fps(int fps) {
	struct timeval now;
	long long ms;

	assert(fps != 0);
	gettimeofday(&now, 0);
	ms = (1000/fps) - dt_ms(now, last_display_time);
	if (ms > 0) {	/* more than 30 fps...give the CPU a rest */
		usleep(ms*1000);
		return 1;
	}

	last_display_time = now;
	return 0;
}

/*
 * Number of seconds elapsed between t1 and t2
 */
int
elapsed_time(struct timeval t1, struct timeval t2) {
	return t2.tv_sec - t1.tv_sec;
}

/*
 * Read a background in from a given png file.
 */
int
set_background(char *fn, int w, int h) {
	int dx, dy;

	background.pm = read_pnm_image(fn, &dx, &dy);
	if (dx != w || dy != h) {
		fprintf(stderr, "set_background: wrong size, given: %dx%d, expected %dx%d\n",
			dx, dy, w, h);
		return 0;
	}
	background_width = w;
	background_height = h;
	background.r = (Rectangle){{0,0}, {w,h}};
	have_background = 1;
	return 1;
}

void
clear_to_background(Rectangle r) {
	int y, w = DX(r);

	if (!have_background) {
		fill_rect(r, Black);
		return;
	}

	for (y=r.min.y; y<r.max.y; y++) {
		Pixel *pp = &background.pm[y*background_width + r.min.x];
		write_row((Point){r.min.x,y}, w, pp);
	}
}

void
clear_screen_to_background(void) {
	clear_to_background(background.r);
}

/*
 * srandomdev:
 *
 * Many programs choose the seed value in a totally predictable manner.
 * This often causes problems.  We seed the generator using the much more
 * secure random(4) interface.  Note that this particular seeding
 * procedure can generate states which are impossible to reproduce by
 * calling srandom() with any value, since the succeeding terms in the
 * state buffer are no longer derived from the LC algorithm applied to
 * a fixed seed.
 */

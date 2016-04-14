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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>

#include "libutil/util.h"
#include "libio/io.h"
#include "font.h"
#include "button.h"

/*
 * The buttons are stored in a linked list.
 */
button *buttons = 0;

int button_sep = 2;

/*
 * Handy global pointer, referring to the last button processed.
 */
button *last_button = 0;

Pixel
off_color(Pixel color) {
	Pixel c = {0,0,0,Z};
	c.r = 3*color.r/2;
	c.g = 3*color.g/2;
	c.b = 3*color.b/2;
	return c;
}

Pixel
dim_color(Pixel color) {
	Pixel c = {0,0,0,Z};
	c.r = color.r/2;
	c.g = color.g/2;
	c.b = color.b/2;
	return c;
}

/*
 * This routine is slow, but safe.  It should be improved.
 */
void
b_fill_rect(PixMap pm, Rectangle r, Pixel color) {
	int x, y;

	if (!rectinrect(r, pm.r))
		return;

	for (y=r.min.y; y<r.max.y; y++) {
		Pixel *rp = PIXMAPADDR(pm, r.min.x, y);

		for (x=r.min.x; x<r.max.x; x++)
			*rp++ = color;
	}
}

int default_button_state = Off;
int default_category = 0;

extern	void check(char *s);

button *
add_button(Rectangle r, char *name, char *default_label, Pixel color,
    void *param) {
	button *b = (button *)malloc(sizeof(button));
	utf8 *label[bstates];

	assert(b);	// allocating button memory
	b->type = Button;
	b->r = r;
	b->name = name;
	b->state = default_button_state;
	b->next = 0;
	b->param = param;
	b->category = default_category;
	last_button = b;

	if ( !(ptinrect(r.min, screen_rect) &&
	    r.max.x <= screen_rect.max.x &&
	    r.max.y <= screen_rect.max.y)) {
		fprintf(stderr, "*** button %s is off screen, ignored:  %d,%d - %d,%d\n",
			name, r.min.x, r.min.y, r.max.x, r.max.y);
		return 0;
	}

	/* lookup configuration file alternates for our default label names. */

	label[Off] = lookup(name, ".off", default_label);
	label[On] = lookup(name, ".on", default_label);
	label[Unavailable] = lookup(name, ".unavail", default_label);

fprintf(stderr, "0 ");
	make_PixMap(&b->pm[Off],	(Rectangle){{0,0},{DX(r),DY(r)}}, color);
fprintf(stderr, "1 ");
	make_PixMap(&b->pm[On],		(Rectangle){{0,0},{DX(r),DY(r)}}, color);
fprintf(stderr, "1 ");
	make_PixMap(&b->pm[Unavailable],(Rectangle){{0,0},{DX(r),DY(r)}}, dim_color(color));
fprintf(stderr, "2\n");

	b_fill_rect(b->pm[Off], inset(b->pm[Off].r, BUTTON_RIM), dim_color(color));
	b_fill_rect(b->pm[On], inset(b->pm[On].r, BUTTON_RIM), Black);

	string_on_pixmap(&(b->pm[Off]), label[Off], White, 1, 1);
	string_on_pixmap(&(b->pm[On]), label[On], White, 1, 1);
	string_on_pixmap(&(b->pm[Unavailable]), label[Unavailable], dim_color(White), 1, 1);

	/* Add button to the end of the linked list */

fprintf(stderr, "2 ");
	if (buttons == 0)
		buttons = b;
	else {
		button *bp = buttons;
		while (bp->next)
			bp = bp->next;
		bp->next = b;
	}
	return b;
}

#ifdef notdef

void
add_slider(Rectangle r, char *name, char *default_label, Pixel color,
    b_func *func, int value) {
	button *b = (button *)malloc(sizeof(button));
	utf8 *label[bstates];
	assert(b);	// allocating button memory
	b->type = Slider;
	b->r = r;
	b->size = (Rectangle){{0,0}, {DX(r), DY(r)}};
	b->name = name;
	b->next = 0;
	b->func = func;
	b->init = (void *)0;
	b->value = value;
	last_button = b;

	if ( !(ptinrect(r.min, screen_rect) &&
		r.max.x <= screen_rect.max.x &&
		r.max.y <= screen_rect.max.y))
		return;

	/* lookup configuration file alternates for our default label names. */

	label[Off] = lookup(name, 0, default_label);

	b->pixels[Off] = make_PixMap(b->size.max);

	place_text(b->size.max, (void *)b->pixels[Off], label[Off], White);

	/* Add button to the end of the linked list */

	if (buttons == 0)
		buttons = b;
	else {
		button *bp = buttons;
		while (bp->next)
			bp = bp->next;
		bp->next = b;
	}
}
#endif

char *
show_rgb(Pixel p) {
	static char buf[100];
	snprintf(buf, sizeof(buf), "%.02x%.02x%.02x", p.r, p.g, p.b);
	return buf;
}

#ifdef notdef
void
dump_pm(Rectangle r, Pixel p[r.max.y][r.max.x]) {
	int x, y;	
	for (y=r.max.y-1; y>=0; y--) {
		fprintf(stderr, "%3d  ", y);
		for (x=0; x<r.max.x; x++) {
			int r = p[y][x].r;
			fprintf(stderr, "%s", (r == 255) ? "X" : " ");
		}
		fprintf(stderr, "\n");
	}
}
#endif

/*
 * Paint the button.  If it is hidden, blacken the area.  If they don't want us
 * to blacken the area, they shouldn't call us in the first place.
 */
void
paint_button(button *b) {
	assert(b->state >= 0 && b->state < bstates);
	if (b->state == Hidden) {
		fill_rect(b->r, Black);
		return;
	}
	write_pixmap(b->r.min,
	    (Rectangle){{0,0}, {DX(b->pm[b->state].r),DY(b->pm[b->state].r)}},
	    b->pm[b->state]);
}

/*
 * Return a button's rectangle the same size and shape as the given rectangle,
 * just below it.
 */
Rectangle
below(Rectangle br) {
	Rectangle r;

	r.max.y = br.min.y - button_sep;
	r.min.y = r.max.y - DY(br);
	r.max.x = br.max.x;
	r.min.x = br.min.x;
	return r;
}

/*
 * Return a button's rectangle the same size and shape as the given rectangle,
 * just below it.
 */
Rectangle
above(Rectangle br) {
	Rectangle r;

	r.min.y = br.max.y + button_sep;
	r.max.y = r.min.y + DY(br);
	r.max.x = br.max.x;
	r.min.x = br.min.x;
	return r;
}

/*
 * Return a button's rectangle the same size and shape as the given rectangle,
 * just to the right of it.
 */
Rectangle
right(Rectangle br) {
	Rectangle r;

	r.min.x = br.max.x + button_sep;
	r.max.x = r.min.x + DX(br);
	r.max.y = br.max.y;
	r.min.y = br.min.y;
	return r;
}

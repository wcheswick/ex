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
#include "fancy_button.h"

int screen_width;
int screen_height;


#ifdef notdef		// attic
char *
show_rgb(Pixel p) {
	static char buf[100];
	snprintf(buf, sizeof(buf), "%.02x%.02x%.02x", p.r, p.g, p.b);
	return buf;
}

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

/*
 * This routine is slow, but safe.  It should be improved.
 */
static void
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

static Pixel
dim_color(Pixel color) {
	Pixel c;
	c.r = color.r/2;
	c.g = color.g/2;
	c.b = color.b/2;
	return c;
}
#endif

/*
 * Paint the button.  If it is hidden, show the background behind it. 
 */
void
paint_fancy_button(button *b) {
	assert(b->state >= 0 && b->state < bstates);
	switch (b->state) {
	case On:
	case Off:
		write_pixmap(b->r.min,
		    (Rectangle){{0,0}, {DX(b->pm[b->state].r),DY(b->pm[b->state].r)}},
		    b->pm[b->state]);
		break;
	case Hidden:
	case Unavailable:
		clear_to_background(b->r);
	}
}

#ifdef notdef
static void
make_old_button(PixMap *pm, Rectangle r, Pixel color, char *text) {
	make_PixMap(pm, r, color);
	b_fill_rect(*pm, inset(r, BUTTON_RIM), dim_color(color));
	string_on_pixmap(pm, text, White, 1, 1);
}
#endif

/*
 * Add a button.  Note: if the Y value is negative, it is from the top of the
 * screen.
 */
button *
add_button_with_images(Point p, PixMap pm_up, PixMap pm_down, void *param) {
	button *b = (button *)malloc(sizeof(button));
	assert(b);	// allocating button memory

	b->pm[Off] = pm_up;
	b->pm[On] = pm_down;

	b->type = Button;
	if (p.y < 0)
		p.y = screen_height - (-p.y) - 1;
	b->r.min = p;
	b->r.max.x = b->r.min.x + b->pm[Off].r.max.x;
	b->r.max.y = b->r.min.y + b->pm[Off].r.max.y;
	b->state = default_button_state;
	b->category = default_category;
	b->next = 0;
	b->param = param;
	last_button = b;

	if ( !(ptinrect(b->r.min, screen_rect) &&
	    b->r.max.x <= screen_rect.max.x &&
	    b->r.max.y <= screen_rect.max.y)) {
		fprintf(stderr, "*** button is off screen, ignored:  %d,%d - %d,%d\n",
			b->r.min.x, b->r.min.y, b->r.max.x, b->r.max.y);
		return 0;
	}

	/* Add button to the end of the linked list */

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

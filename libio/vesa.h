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

#define VESA_Y(y)	(vesa_max_y - (y) - 1)	// flip the Y axis

enum sprite_status {
	Undef,
	On,
	Off,
	TempOff,
};

typedef struct sprite sprite;
struct sprite {
	enum sprite_status status;	
	Point	size;		// size of sprite
	Point	offset;		// pointing part of the sprite, from lower left
	Pixel32	*image;		// sprite definition, with alpha channel
	Rectangle used;		// portion of sprite used
	Rectangle behind;	// portion of screen obscured by sprite
	Pixel32	*back;		// copy of screen under 'behind'
};

extern	int vesa_max_x;
extern	int vesa_max_y;
extern	int vesa_bytes_per_pixel;

extern	char *vesa_init(int w, int h);
extern	void vesa_end(void);
extern	void vesa_draw_box(Rectangle box, Pixel color);
extern	void vesa_fill_box(Rectangle box, Pixel color);
extern	void vesa_clear(Pixel color);
extern	void vesa_put_line(Point p, Pixel *line, int n_pixels);
extern	int vesa_check_hardware(int w, int h);
extern	void vesa_hide_cursor(void);
extern	void vesa_show_cursor(void);
extern	int vesa_check_mouse(Rectangle);
extern	void vesa_restore_mouse(int);
extern	int vesa_dump_screen(char *fn);
extern	void sprite_on(sprite *s, Point p);
extern	void sprite_off(sprite *s);

extern	sprite mouse_sprite;
extern	void vesa_setmouse(int w, int h, u_char *cursor_mask, Pixel color);
extern	void vesa_mouseto(Point p);

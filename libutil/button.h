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
 *
 * So there.
 */

#define BUTTON_RIM	2	// the bright edge around the button

typedef enum btype btype;
enum btype {
	Button,
	Slider
};

typedef enum bstate bstate;
enum bstate {
	Off,
	On,
	Unavailable,
	Hidden
};
#define bstates	(Hidden+1)

typedef struct button	button;
struct button {
	PixMap	pm[bstates];
	btype	type;
	char	*name;
	Rectangle r;		/* button location and size */
	bstate	state;
	int	value;
	void	*param;
	int	category;
	button	*next;
};

extern	font *button_font;

extern	void paint_button(button *b);
extern	button *add_button(Rectangle r, char *name, char *default_label, Pixel color,
	    void *param);
extern	Rectangle below(Rectangle r);
extern	Rectangle above(Rectangle r);
extern	Rectangle right(Rectangle r);

extern	int button_height;
extern	int button_sep;

extern	button *buttons;	// top of linked list of buttons
extern	button *last_button;
extern	int default_button_state;
extern	int default_category;


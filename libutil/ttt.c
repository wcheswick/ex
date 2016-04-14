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

/* font test routine */

#include <sys/types.h>
#include <rune.h>
#include <errno.h>

#include "util.h"
#include "font.h"

Rectangle text_rect;

static void
place_string(int width, int height, u_char bitmap[height][width]) {
}

int
main(int argc, char *argv[]) {
	int i;
	Rectangle r;
	u_char *bitmap, *bp;
	int row, col, bits_left;

	struct font *pelm16  = load_font("../lib/fonts/pelm.ascii.16.bdf");
	struct font *helvB12 = load_font("../lib/fonts/helvB12-ISO8859-1.pcf.bdf");
	struct font *helvR10 = load_font("../lib/fonts/helvR10.pcf.bdf");
	struct font *lubBI18 = load_font("../lib/fonts/lubBI18.pcf.bdf");
	struct font *palB25 = load_font("../lib/fonts/palatino.B.25.1.pcf.bdf");
	struct font *palR8 = load_font("../lib/fonts/palatino.R.8.1.pcf.bdf");
	struct font *timBI08 = load_font("../lib/fonts/timBI08.pcf.bdf");
	struct font *timesB8 = load_font("../lib/fonts/times.B.8.1.pcf.bdf");


	i = render_string(helvB12, "This is a just test", &bitmap, &r);
	printf("rect = ((%d,%d), (%d,%d))\n", r.min.x, r.min.y, r.max.x, r.max.y);

	bp = bitmap; bits_left=8;
	for (row=r.min.y; row<r.max.y; row++) {
		if (row == 0)
			printf("0");
		else
			printf(".");
		for (col=r.min.x; col<r.max.x; col++) {
			if (((*bp) >> (--bits_left)) & 0x1)
				printf("X");
			else
				printf(" ");
			if (bits_left == 0) {
				bp++;
				bits_left = 8;
			}
		}
		if (bits_left != 8) {  // return unused portion of byte for refund
			bp++;
			bits_left = 8;
		}
		if (row == 0)
			printf("0\n");
		else
			printf(".\n");
	}
	return 0;
}

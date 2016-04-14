/*
 * Copyright 2010 Bill Cheswick <ches@cheswick.com>
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
 *	win.c - webcam input.  This is specifically for the TRENDnet TV-IP100
 *
 *	factory settings we like:
 *	
 *	IP: 192.168.0.20
 *	video resolution: 320x240
 *	compression rate: medium
 *
 *	http://192.168.0.20/IMAGE.JPG
 *	returns a single image as http response.
 *
 *	http://192.168.0.20/MJPEG.CGI
 *	feeds motion jpeg data at about 300KB/sec
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>
#include <errno.h>

#include "libutil/util.h"
#include "io.h"

#define WINDEBUG

static image buf_in;
image *video_in;

static int hue = 0;
static int saturation = 0;
static int brightness = 0;
static int contrast = 0;

void
set_hue(char h) {
fprintf(stderr, "set_hue: %d\n", h);
	hue = h;
}

void
set_saturation(u_char s) {
fprintf(stderr, "set_sat: %d\n", s);
	saturation = s;
}

void
set_brightness(char b) {
	brightness = b;
}

void
set_contrast(u_char c) {
	contrast = c;
}

char
get_hue(void) {
	return hue;
}

u_char
get_saturation() {
	return saturation;
}

char
get_brightness(void) {
	return brightness;
}

u_char
get_contrast(void) {
	return contrast;
}

/*
 * Generalized tcpconnect, now with IPv6 support.
 */
int
tcpconnect(char *host, char *service) {
	struct addrinfo hints, *res, *res0;
	int error;
	int s;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, service, &hints, &res0);
	if (error) {
		fprintf(stderr, "ctl: getaddrinfo error: %s\n",
			gai_strerror(error));
		return -1;
	}
	s = -1;
	cause = "no addresses";
	errno = EADDRNOTAVAIL;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			cause = "socket";
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			cause = "connect";
			close(s);
			s = -1;
			continue;
		}
		break;  /* okay we got one */
	}
	if (s < 0) {
		fprintf(stderr, "ctl: connect error: %s\n", cause);
		return -1;
	}
	freeaddrinfo(res0);
	return s;
}

int webfd = -1;

char *webhost;


#ifdef SINGLEFRAMEGRABS

/* This works, but it is too slow.  */
void
init_video_in(void) {
	webfd = tcpconnect("192.168.0.20", "80");
	if (webfd < 0)
		return;
}

#define SNAP_URI	"/IMAGE.JPG"

void
get_single_web_image(void) {
	char buf[100];
	char *cp, *token;
	int content_length, rc, n;
	FILE *web;

fprintf(stderr, "get_single_web_image\n");
	snprintf(buf, sizeof(buf),
		"GET /IMAGE.JPG HTTP/1.1\nUser-Agent: ex\nAccept: */*\n\n");
	write(webfd, buf, strlen(buf));

	web = fdopen(webfd, "r");
	if (web == NULL) {
		perror("web open error");
		fprintf(stderr, "init_video_in: web open error\n");
		return;
	}
	cp = fgets(buf, sizeof(buf), web);
	if (cp == NULL) {
		if (feof(web))
			fprintf(stderr, "get_single_web_image: EOF\n");
		else if (ferror(web)) {
			perror("web open error");
			fprintf(stderr, "get_single_web_image: error\n");
		} else
			fprintf(stderr, "get_single_web_image: mystery end\n");
		return;
	}
	n = sscanf(buf, "%*s %d %*s", &rc);
	if (n < 1 || (rc / 100) != 2) {
		fprintf(stderr, "get_single_web_image: error return: %s\n", buf);
		return;
	}

	content_length = 0;
	do {	cp = fgets(buf, sizeof(buf), web);
#ifdef WINDEBUG
		fprintf(stderr, "%s", &buf[0]);
#endif
		token = strsep(&cp, " \r\n");
		if (token == NULL)
			continue;
		if (strcmp(token, "Content-Type:") == 0) {
			token = strsep(&cp, " \r\n");
			if (strcmp(token, "image/jpeg") != 0) {
				fprintf(stderr, "Wrong Content (%s), skipping\n",
					token);
				return;
			}
		} else if (strcmp(token, "Content-Length:") == 0) {
			token = strsep(&cp, " \r\n");
			content_length = atoi(token);
		}
	} while (isalpha((char)buf[0]));
#ifdef WINDEBUG
	fprintf(stderr, "content_length: %d\n", content_length);
#endif
	if (content_length == 0) {
		fclose(web);
		return;
	}

	decode_jpeg_image(web, &buf_in);
	video_in = &buf_in;
	fclose(web);
}

void
grab_video_in(void) {
	get_single_web_image();
}

void
end_video_in(void) {
	close(webfd);
}

#endif

#ifdef WINDEBUG

char *prog;

int main(int argc, char *argv[]) {
	prog = "test";

	init_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	grab_video_in();
	end_video_in();

	return 0;
}
#endif

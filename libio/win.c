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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>
#include <errno.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "libutil/util.h"
#include "io.h"

// #define WINDEBUG

/* shared memory stuff:
 *
 * Since we consume images at a varying rate, we don't want the
 * network stream of data to back up.  Fork off a reader that reads
 * and decodes images into a ring buffer in shared memory.  If we
 * seem to be getting well ahead, just toss the frame.
 */

#define	RING_SIZE	3	/* ah, for the days of CIO on the CDC */
typedef struct ring_buffer {
	int in, out;
	image buf_in[RING_SIZE];
} ring_buffer;

static int shmid;
pid_t fork_pid;

ring_buffer *ring_buf;

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

#define WEB_CAM		"192.168.0.20"


char *video_boundary=0;
int boundary_string_length;

/*
 * Return a zero if we need to reconnect to the camera.
 */

int
get_next_web_image(FILE *webcam) {
	char buf[100];
	char *cp;
	int content_length = 0;
	int skip = 0;

#ifdef WINDEBUG
fprintf(stderr, ">");
#endif
	/* Find the next video boundary in the input stream, and decode
	 * the jpeg therein.  Note: it is possible that we have been too
	 * slow, and the net or kernel has dropped images. We don't care.
	 * I think.
	 */

	do {	if (feof(webcam))
			return 0;
		cp = fgets(buf, sizeof(buf), webcam);
#ifdef WINDEBUG
		fprintf(stderr, "%s", &buf[0]);
#endif
		if (cp == NULL) {
			fprintf(stderr, "get_next_web_image: connection went away\n");
			fclose(webcam);
			return 0;
		}
	} while (strncmp(&buf[0], video_boundary, boundary_string_length) != 0);
#ifdef WINDEBUG
fprintf(stderr, "|");
#endif

	cp = &buf[boundary_string_length];
	do {	char *token = strsep(&cp, " \r\n");
		if (token == NULL)
			continue;
		if (strcmp(token, "Content-Type:") == 0) {
			token = strsep(&cp, " \r\n");
			if (strcmp(token, "image/jpeg") != 0) {
				fprintf(stderr, "Wrong Content (%s), skipping\n",
					token);
				return 1;
			}
		} else if (strcmp(token, "Content-length:") == 0) {
			token = strsep(&cp, " \r\n");
			content_length = atoi(token);
		}
		cp = fgets(buf, sizeof(buf), webcam);
	} while (isalpha((char)buf[0]));

#define	ADVANCE(x)	((x)+1 % RING_SIZE)

	if (skip || ADVANCE(ring_buf->out) == ring_buf->in) {
		char buf[content_length];
		int n = fread(buf, content_length, 1, webcam);
	} else {
		decode_jpeg_image(webcam, &(ring_buf->buf_in[ring_buf->in]), 0);
		ring_buf->in = ADVANCE(ring_buf->in);
	}
#ifdef WINDEBUG
fprintf(stderr, "!!!\n");
#endif
	return 1;
}

FILE *
start_webcam_video_feed(void) {
	FILE *webcam;
	char *cp;
	char buf[100];
	int n;
	int rc;

#ifdef WINDEBUG
	fprintf(stderr, "?");
#endif

	int webfd = tcpconnect(WEB_CAM, "80");
	if (webfd < 0) {
		perror("connect to web cam");
		return NULL;
	}
#ifdef WINDEBUG
	fprintf(stderr, ".");
#endif

	/* start the HTTP ball rolling */

	snprintf(buf, sizeof(buf),
		"GET /MJPEG.CGI HTTP/1.1\nUser-Agent: ex\nAccept: */*\n\n");
	n = write(webfd, buf, strlen(buf));

	webcam = fdopen(webfd, "r");
	if (webcam == NULL) {
		perror("web open error");
		fprintf(stderr, "init_video_in: web open error\n");
		return NULL;
	}

	/* Get the http response */

	cp = fgets(buf, sizeof(buf), webcam);
	if (cp == NULL) {
		if (feof(webcam))
			fprintf(stderr, "init_video_in: EOF\n");
		else if (ferror(webcam)) {
			perror("reading web response");
			fprintf(stderr, "init_video_in: error\n");
		} else
			fprintf(stderr, "init_video_in: mystery end\n");
		fclose(webcam);
		return NULL;
	}
	n = sscanf(buf, "%*s %d %*s", &rc);
	if (n < 1 || (rc / 100) != 2) {
		fprintf(stderr, "init_video_in: error return: %s\n", buf);
		fclose(webcam);
		return NULL;
	}

	/* HTTP is happy.  Make sure this is the right type of connection,
	 * and get the video boundary between frames.
	 */

	do {	char *cp;
		cp = fgets(buf, sizeof(buf), webcam);
#ifdef WINDEBUG
//		fprintf(stderr, "%s", &buf[0]);
#endif
		char *token = strsep(&cp, " \r\n");
		if (token == NULL)
			break;;
		if (strcmp(token, "Content-Type:") == 0) {
			token = strsep(&cp, ";");
			if (!token || strcmp(token, "multipart/x-mixed-replace") != 0) {
				fprintf(stderr, "Wrong Content (%s), bailing\n",
					token);
				fclose(webcam);
				return NULL;
			}
			token = strsep(&cp, "=");
			if (!token || strcmp(token, "boundary") != 0) {
				fprintf(stderr, "content boundary missing (%s), bailing\n",
					token);
				fclose(webcam);
				return NULL;
			}
			token = strsep(&cp, "\r\n");
			if (!token) {
				fprintf(stderr, "content boundary is null, bailing\n");
				fclose(webcam);
				return NULL;
			}
			video_boundary = strdup(token);
		}
	} while (!video_boundary);
	boundary_string_length = strlen(video_boundary);
#ifdef WINDEBUG
	fprintf(stderr, "boundary='%s'\n", video_boundary);
#endif
	return webcam;
}

void
clean_up(void) {
	struct shmid_ds buf;
	shmdt(ring_buf);
	shmctl(shmid, IPC_RMID, &buf);
}

void
child_done(int i) {
	int status;

	fprintf(stderr, "parent: child gone\n");
	waitpid(fork_pid, &status, 0);
	clean_up();
	exit(1);
}

void
init_video_in(void) {
	FILE *webcam;

	/*
	 * We set up a continuous reader in a separate process.  We store
	 * the ring buffer of incoming images in shared memory.
	*/

	if ((shmid = shmget(IPC_PRIVATE, sizeof(ring_buffer), 0644 | IPC_CREAT)) ==
 -1) {
		perror("shmget");
		exit(2);
	}
	
	ring_buf = shmat(shmid, (void *)0, 0);
	if ((char *)ring_buf == (char *) (-1)) {
		perror("shmat");
		exit(3);
	}
	ring_buf->in = ring_buf->out = 0;

	signal(SIGCHLD, child_done);

	switch(fork_pid = fork()) {
	case -1:
		perror("fork");
		exit(4);
	case 0:
		ring_buf = shmat(shmid, (void *)0, 0);
		if ((char *)ring_buf == (char *) (-1)) {
			perror("shmat");
			exit(3);
		}
fprintf(stderr, "Child started, ring_buf=%p\n", ring_buf);
		while ((webcam=start_webcam_video_feed()) != NULL) {
			while (get_next_web_image(webcam))
				;
		}
fprintf(stderr, "Child terminating\n");
		exit(1);
	}
}

void
grab_video_in(void) {
	while (ring_buf->out == ring_buf->in)
		usleep(1);
	video_in = &(ring_buf->buf_in[ring_buf->out]);
fprintf(stderr, "-");
	ring_buf->out = ADVANCE(ring_buf->out);
}

void
end_video_in(void) {
	// kill and reap child.  What a lovely sentiment!

	kill(fork_pid, SIGINT);
	clean_up();
}

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

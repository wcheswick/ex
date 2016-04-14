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


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/time.h>

#include <math.h>

#include <audiofile.h>

#include "libutil/util.h"
#include "aud.h"

int dsp_fd = -1;

void
init_audio(void) {
	int format = AFMT_S16_LE;	/* signed 16-bit little-endian */
	int channels = 1;
	int rate = AUDIO_RATE;

	dsp_fd = open(AUDIO_DEV, O_RDWR, 0);
	if (dsp_fd < 0) {
		perror(AUDIO_DEV);
		exit(1);
	}

	
	if (ioctl(dsp_fd, SNDCTL_DSP_RESET, 0) == -1) {
		perror(AUDIO_DEV);
		exit(2);
	}
	
	if (ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		perror(AUDIO_DEV);
		exit(2);
	}
	if (format != AFMT_S16_LE) {
		fprintf(stderr,
			"init_audio, sound format AFMT_S16_LE not supported\n");
		exit(3);
	}
  
	if (ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
		perror(AUDIO_DEV);
		exit(4);
	}
	if (channels != AUDIO_CHANNELS) {
		fprintf(stderr,
			"init_audio, %d channel(s) not supported\n",
				AUDIO_CHANNELS);
		exit(5);
	}

	if (ioctl(dsp_fd, SNDCTL_DSP_SPEED, &rate) == -1) {
		perror(AUDIO_DEV);
		exit(6);
	}
	if (abs(AUDIO_RATE - rate) > 100) {
		fprintf(stderr,
			"init_audio, audio rate %d not supported\n", AUDIO_RATE);
		exit(7);
	}
}

/*
 * Return the actual number of samples read, or a read error code
 */
int
read_mic_samples(sample *buf, int nsamp) {
	int i, n=0;

	for (i=0; i<nsamp; ) {
		int remaining_bytes = (nsamp - i)*sizeof(sample);
		n = read(dsp_fd, &buf[i], remaining_bytes);
		if (n <= 0)
			break;
		i += n/sizeof(sample);
	}
	if (i == nsamp)
		return i;
	else
		return n;
}

char *set_volume = 0;

int output_volume = -1;

void
set_output_volume(int v) {
	char buf[100];

	if (!set_volume)
		set_volume = lookup("volume", 0, "mixer vol %d:%d");
	if (v >= -1 && v <= 100) {
		if (v >= 0)
			output_volume = v;
		else
			v = 0;	// shut it off 
		snprintf(buf, sizeof(buf), set_volume, v, v);
		system(buf);
	}
}

char *set_mic = 0;

int microphone_volume = -1;

void
set_microphone_volume(int v) {
	char buf[100];

	if (!set_mic)
		set_mic = lookup("microphone", 0, "mixer mic %d:%d rec %d:%d >/dev/null");
	if (v >= 0 && v <= 100) {
		snprintf(buf, sizeof(buf), set_mic, v, v, v, v);
		system(buf);
		microphone_volume = v;
	}
}

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

/*
 * aud.c - audio display and teaching station.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>
// #include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <math.h>
// #include <kvm.h>
#include <errno.h>
#include <assert.h>
#include <sys/soundcard.h>
#include <complex.h>

// #include <fftw3.h>

// #include <audiofile.h>

#include "libio/arg.h"
#include "libutil/util.h"
#include "libio/io.h"
#include "libutil/font.h"
#include "libutil/button.h"
#include "aud.h"

#define AUD_SAMP_SIZE	512
#define FFT_SIZE	AUD_SAMP_SIZE

int debug = 0;
int refresh_screen = 1;
int option = 0;
int logscale = 0;

char *title;
char *subtitle;

Rectangle title_r;
Rectangle subtitle_r;

Rectangle outer_volume_r, volume_r;
Rectangle outer_fft_r, fft_r;
Rectangle outer_wave_r, wave_r;
Rectangle buttons_r;
Rectangle load_r;
Rectangle settings_r;

Pixel screen[SCREEN_HEIGHT][SCREEN_WIDTH];


enum {
	Cat_none = 0,
	Cat_birds,
	Cat_people,
	Cat_tech,
	Cat_other,
};

Pixel catcolor[] = {
	Black,
	Green,
	LightBlue,
	Yellow,
	Magenta,
	LightGreen,
	Orange,
};

font *text_font;
font *button_font;
font *title_font;
font *subtitle_font;

button *mic_button;
button *now_playing = 0;

struct sound_info {
	char *path;
	int nframes;
};
typedef struct sound_info sound_info;

struct sounds {
	char	*name;
	int	category;
	char	*label;
	char	*file;
	sound_info *si;
} sounds[] = {
//	{"sintest",	Cat_tech,	"X|sine wave",			"sinx.wav"},	
	{"sin440",	Cat_tech,	"440 Hz|sine wave",		"sin-440.wav"},	
	{"sin880",	Cat_tech,	"880 Hz|sine wave",		"sin-880.wav"},	
	{"sq440",	Cat_tech,	"440 Hz|square wave",		"sq-440.wav"},	
	{"airport",	Cat_people,	"M\303\274nchen|airport",	"munich-crowd.wav"},	
	{"chimes",	Cat_people,	"M\303\274nchen|chimes",	"munich-chimes.wav"},	
	{"glock",	Cat_people,	"M\303\274nchen|Glockenspiel",	"munich-glock.wav"},	
	{"gulls",	Cat_birds,	"Sea gulls",			"gulls.wav"},	
	{"yearlings",	Cat_birds,	"Yearling gulls|whining",	"yearlinggulls.wav"},	
	{"bbdove",	Cat_birds,	"Zebra dove",			"bbdove.wav"},	
	{"bbrooster",	Cat_birds,	"Rooster",			"bbrooster.wav"},	
	{"bbird",	Cat_birds,	"Bernardsville|bird",		"bville-bird.wav"},	
	{"frogs",	Cat_other,	"Australian|frogs",		"vic-cricks2.wav"},	
	{"frogs2",	Cat_other,	"Australian|frogs 2",		"vic-cricks2.wav"},	
	{"frogs3",	Cat_other,	"Floridian|tree frogs",		"treefrogs3.wav"},	
	{0, 0, 0}	
};

AFfilehandle src = 0;	// audio file source, or 0 if microphone
int frame, nframes;
int audio_out_buffer_space;

#define BUTTON_SEP	5
#define BUTTON_WIDTH	200	// the area containing the input select buttons
#define BUTTON_TITLE_SEP 20
#define BUTTON_HEIGHT	38	/* pixels */

#define DESIRED_QUEUED_MS	0

int queued = 0;		// ms of queued sound output
int starved = 0;
l_time_t last_checked = 0;

sound_info *
get_sound_info(char *fn) {
	char *path, *p = find_file(fn, "lib/aud");
	static struct sound_info *isp;
	int nchannels;
	int rate;
	double length;
	AFfilehandle s;

	if (!p) {
		fprintf(stderr, "%-23s ****	file not found\n", fn);
		return 0;
	}
	path = strdup(p);

	s = afOpenFile(path, "r", 0);
	if (s == AF_NULL_FILEHANDLE) {
		switch (errno) {
		case AF_BAD_OPEN:
			fprintf(stderr, "Could not open file %s\n", path);
			break;
		case AF_BAD_FILEFMT:
			fprintf(stderr, "Unknown file format, file %s\n", path);
			break;
		default:
			fprintf(stderr, "AfOpen error %d, file %s\n", errno, path);
		}
		return 0;
	}

	nchannels = afGetChannels(s, AF_DEFAULT_TRACK);
	rate = afGetRate(s, AF_DEFAULT_TRACK);
	nframes = afGetFrameCount(s, AF_DEFAULT_TRACK);
	length = (double)nframes/AUDIO_RATE;
	if (rate != AUDIO_RATE || nchannels != 1) {
		fprintf(stderr, "%-23s ch %d  rate %6d  %10.1f sec.  ** format mismatch**\n",
			fn, nchannels, rate, length);
		return 0;
	}
	fprintf(stderr, "%-23s ch %d  rate %6d  %10.1f sec.\n",
		fn, nchannels, rate, length);

	isp = (sound_info *)malloc(sizeof(sound_info));
	assert(isp);	/* allocating sound information */
	isp->path = path;
	isp->nframes = nframes;
	return isp;
}

button *last_category_button = 0;

static void
paint_category(int category, int state) {
	button *bp = buttons;

	while (bp) {
		if (bp->category == category) {
			if (bp->value == -1 || sounds[bp->value].si)
				bp->state = state;
			else
				bp->state = Unavailable;
			paint_button(bp);
			flush_screen();
		}
		bp = bp->next;
	}
}

/*
 * We keep the previous source when we select a new category, even if
 * the button is not showing.  I wonder if this is a poor design choice.
 */

int
set_category(button *bp) {
	int category = bp->category;
	if (last_category_button) {
		paint_category(last_category_button->category, Hidden);
		last_category_button->state = Off;
		paint_button(last_category_button);
	}
	last_category_button = bp;
	if (category) {
		paint_category(category, Off);
		bp->state = On;
		paint_button(bp);
	}
	return 1;
}

int
set_file_source(button *bp) {
	AFfilehandle s;
	struct sounds *sp = &sounds[bp->value];

	assert(sp->si->path);	// we should have something to play at this point

	s = afOpenFile(sp->si->path, "r", 0);
	if (s == AF_NULL_FILEHANDLE) {
		// should never happen, we checked when we started
		return 0;
	}

#define	BPB	8	/* bits per byte */

	afSetVirtualSampleFormat(s, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP,
		sizeof(sample)*BPB);

	set_output_volume(output_volume);
	src = s;
	frame = 0;
	nframes = sp->si->nframes;
	ioctl(dsp_fd, AIONWRITE, &audio_out_buffer_space);
	return 1;
}

void
select_sound_file(button *bp) {
	if (set_file_source(bp)) {
		if (now_playing && now_playing->state != Hidden) {
			now_playing->state = Off;
			paint_button(now_playing);
		}
		bp->state = On;
		paint_button(bp);
		now_playing = bp;
	}
}

void
select_mic(void) {
	if (src)
		afCloseFile(src);
	src = 0;
	ioctl(dsp_fd, SOUND_PCM_RESET, 0);  // XXX this is probably better

//	ioctl(dsp_fd, SOUND_PCM_SYNC, 0);
	set_output_volume(40);
	if (now_playing && now_playing->state != Hidden) {
		now_playing->state = Off;
		paint_button(now_playing);
	}
	mic_button->state = On;
	paint_button(mic_button);
	now_playing = mic_button;
}

int
do_exit(char *param) {
	end_display();
	//end_ao();
	exit(0);
}

#define BI(p) ((void *)p)

/*
 * The X routines appear to be unable to keep up with our graphic needs.
 * So, for the X display, we write a single, scanning column for the
 * volume and frequency displays, greatly reducing the display I/O needs,
 * and keeping the X display useful for debugging.
 *
 * We use xout for both displays, assuming that one is on top of the other,
 * and has the same size.
 */
int xout;	// used for scanning output on X debugging display

/*
 * Figure out where everything goes.
 */
void
layout_screen(void) {
	Rectangle r;
	int category;
	Rectangle exit_r = (Rectangle){{SCREEN_WIDTH-50, 0},
		{SCREEN_WIDTH, BUTTON_HEIGHT}};
	Rectangle left_col, right_col, subcat_right, subcat_left;

	text_font = load_font(TF);
	button_font = load_font(BF);
	title_font = load_font(TF);
	subtitle_font = load_font(STF);

	title_r = (Rectangle){{0, SCREEN_HEIGHT - FONTHEIGHT(title_font)},
		{SCREEN_WIDTH, SCREEN_HEIGHT}};
	subtitle_r = (Rectangle){{0, title_r.min.y - 3*FONTHEIGHT(subtitle_font)},
		{SCREEN_WIDTH, title_r.min.y}};
	buttons_r = (Rectangle){{SCREEN_WIDTH-BUTTON_WIDTH, exit_r.max.y},
		{SCREEN_WIDTH, subtitle_r.min.y - 3}};

	title = lookup("title", 0, "What Does Sound Look Like?");
	subtitle = lookup("subtitle", 0, "");

	outer_fft_r.max = (Point){SCREEN_WIDTH-BUTTON_WIDTH, buttons_r.max.y};
	outer_fft_r.min = (Point){0, outer_fft_r.max.y - 256};

	outer_volume_r.max.x = outer_fft_r.max.x;
	outer_volume_r.max.y = outer_fft_r.min.y;
	outer_volume_r.min.x = outer_fft_r.min.x;
	outer_volume_r.min.y = outer_volume_r.max.y - 100;

	outer_wave_r.max.x = outer_volume_r.max.x;
	outer_wave_r.max.y = outer_volume_r.min.y;
	outer_wave_r.min.x = outer_volume_r.min.x;
	outer_wave_r.min.y = outer_wave_r.max.y - 100;

	fft_r = inset(outer_fft_r, 3);
	volume_r = inset(outer_volume_r, 3);
	wave_r = inset(outer_wave_r, 3);

	xout = fft_r.min.x;

	load_r.max.y = outer_wave_r.min.y - 2;
	load_r.max.x = outer_wave_r.max.x;
	load_r.min.x = outer_wave_r.min.x;
	load_r.min.y = load_r.max.y - FONTHEIGHT(text_font) - 3;
	set_font(button_font);

	settings_r.min.x = 0;
	settings_r.min.y = 0;
	settings_r.max.x = exit_r.min.x;
	settings_r.max.y = FONTHEIGHT(text_font) +2;

	button_sep = BUTTON_SEP;

	r = buttons_r;
	r.min.y = r.max.y - BUTTON_HEIGHT;
	default_category = Cat_none;

	mic_button = add_button(r, "microphone", "Microphone", Red, BI(select_mic));
	mic_button->value = -1;

	left_col.min.x = buttons_r.min.x;
	left_col.max.x = buttons_r.min.x + DX(buttons_r)/2 - 1;
	left_col.max.y = r.min.y - BUTTON_SEP;
	left_col.min.y = left_col.max.y - BUTTON_HEIGHT;

	right_col.min.x = buttons_r.min.x + DX(buttons_r)/2 + 1;
	right_col.max.x = buttons_r.max.x;
	right_col.max.y = left_col.max.y;
	right_col.min.y = left_col.min.y;

	/* Enter category buttons */

	default_button_state = Off;

	add_button(left_col, "bird", "Birds", catcolor[Cat_birds], BI(set_category));
	last_button->category = Cat_birds;
	last_button->value = -1;
	add_button(below(last_button->r), "people", "People/crowds",
		catcolor[Cat_people], BI(set_category));
	last_button->category = Cat_people;
	last_button->value = -1;
	left_col = below(last_button->r);

	add_button(right_col, "technical", "Technical",
		catcolor[Cat_tech], BI(set_category));
	last_button->category = Cat_tech;
	last_button->value = -1;
	add_button(below(last_button->r), "other", "Other",
		catcolor[Cat_other], BI(set_category));
	last_button->category = Cat_other;
	last_button->value = -1;
	right_col = below(last_button->r);
	/* Now lay out the buttons for each category */
	default_button_state = Hidden;

	for (category=Cat_other; category>Cat_none; category--) {
		int i, count=0;
		Pixel color = catcolor[category];

		default_category = category;
		subcat_right = right_col;
		subcat_left = left_col;

		for (i=0; sounds[i].name; i++) {
			if (sounds[i].category != category)
				continue;
			if (count++ & 1)
				r = subcat_right = below(subcat_right);
			else
				r = subcat_left = below(subcat_left);
			add_button(r, sounds[i].name,
			    sounds[i].label, color, BI(select_sound_file));
			sounds[i].si = get_sound_info(sounds[i].file);
			last_button->value = i;
			last_button->state = Hidden;
		}
	}

	default_category = Cat_none;
	default_button_state = Off;
	set_font(text_font);
}
#ifdef notdef
kvm_t kvm = 0;
#endif

int cpu_states[CPUSTATES];

void
show_load(void) {
	char buf[30];
	double loads[1];

#ifdef notdef
	if (kvm == 0) {
		char errbuf[_POSIX2_LINE_MAX];
		kvm = kvm_openfiles("/kernel", NULL, NULL, O_RDONLY, errbuf);
		if (kvm == NULL) {
			fprintf(logfd, "kvm open failure: %s\n", errbuf);
			end_display();
			//end_ao();
		}
	}
#endif
return;
	if (getloadavg(loads, 1) < 0)
		return;
return;
	snprintf(buf, sizeof(buf), "load average: %4.1f", loads[0]);
	write_string(load_r, White, buf);
}

void
show_volume(void) {
	char buf[100];

	set_font(text_font);
	snprintf(buf, sizeof(buf), "vol %3d    mic %3d",
		output_volume, microphone_volume);
	write_string(settings_r, White, buf);
}

/*
 * Draw the current screen.
 */
void
draw_screen(void) {
	button *bp = buttons;

	while (bp) {
		if (bp->state != Hidden) {
			paint_button(bp);
		}
		bp = bp->next;
	}

	set_font(title_font);
	write_centered_string(title_r, White, title);
	set_font(subtitle_font);
	write_centered_string(subtitle_r, White, subtitle);
	flush_screen();
	draw_rect(outer_fft_r, White);
	draw_rect(outer_volume_r, White);
	draw_rect(outer_wave_r, White);
	show_load();
	show_volume();
	show_cursor();
	flush_screen();
}

void
kbd_hit(char key) {
	switch (key) {
	case 'd':
		dump_screen(0);
		break;
	case 'm':
		set_microphone_volume(microphone_volume-1);
		break;
	case 'M':
		set_microphone_volume(microphone_volume+1);
		break;
	case 'o':
		option = 1 - option;
		break;
	case 'v':
		set_output_volume(output_volume-1);
		break;
	case 'V':
		set_output_volume(output_volume+1);
		break;
	case 'q':
	case 'x':
	case 'X':	/* exit the program */
		// end_ao();
		end_display();
		exit(0);
	}
	show_volume();
}

void
click(Point mouse) {
	button *bp = buttons;
	void (*func)(button *bp);

	while (bp && (bp->state == Hidden || !ptinrect(mouse, bp->r)))
		bp = bp->next;
	if (!bp)
		return;

	if (bp->state == Unavailable)
		return;

	func = bp->param;
	func(bp);
}

/*
 * A lookup table to show the spectrum on a logarithmic scale.
 */
int map_fft_y[SCREEN_HEIGHT];

int fft_ysize;

#ifdef ORIG
#define	MIN_FREQ	20.0	// Hz
#define MAX_FREQ	11000.0	// Hz
#else
#define	MIN_FREQ	20.0	// Hz
#define MAX_FREQ	6000.0	// Hz
#endif

void
init_input_graph(void) {
	double log_min_freq = log(MIN_FREQ);
	double log_max_freq = log(MAX_FREQ);
	double log_range = log_max_freq - log_min_freq;
	int ysize = DY(fft_r);
	double log_freq_incr = log_range/(double)(ysize+1);
	double log_freq = log_min_freq;
	double top_freq = AUDIO_RATE/2 + 1;
	double freq_per_input = top_freq/(double)AUD_SAMP_SIZE;
	int y;

	fft_ysize = ysize;

	for (y=0; y<fft_ysize; y++, log_freq += log_freq_incr) {
		double freq = exp(log_freq);
		int input = freq/freq_per_input;

		if (input > AUD_SAMP_SIZE/2+1)	// cheap hack
			input = AUD_SAMP_SIZE/2+1;
		assert(input >= 0 && input <= AUD_SAMP_SIZE/2 + 1);
		assert(y >= 0 && y < ysize);
assert(freq > 0.1);
		if (logscale)
			map_fft_y[y] = input;
		else
			map_fft_y[y] = y;
	}
}

fftw_plan plan;
double in[FFT_SIZE];
fftw_complex out[FFT_SIZE];

void
init_fft(void) {
	plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, 0);
}

int
usage(void) {
	fprintf(stderr, "usage: aud [-d] [-h]\n");
	return 13;
}

char *prog;
int check_hardware = 0;

int
main(int argc, char *argv[]) {
	prog = argv[0];

	load_config("aud");
	init_font_locale(lookup("locale", 0, 0));

	srandom(time(0));
	set_screen_size(SCREEN_WIDTH, SCREEN_HEIGHT);
	layout_screen();
	init_input_graph();

	init_audio();
	set_microphone_volume(MICROPHONE_VOLUME);
	set_output_volume(-1);

	fprintf(stderr, "testing FFT, wait a moment...\n");
	init_fft();
	init_display(argc, argv, prog);
//	select_mic();

	ARGBEGIN {
	case 'd':	debug++;			break;
	case 'h':	check_hardware++;		break;
	default:
		return usage();
	} ARGEND;

	io_main_loop();
	return 0;
}

int wave_point[SCREEN_WIDTH];
static int first_wave = 1;

void
draw_wave(sample *samples, int n) {
	int npts = DX(wave_r);
	int center_y = wave_r.min.y + DY(wave_r)/2;
	int scale = SHRT_MAX/(DY(wave_r)/2);
	int start;
	int i;

	/*
	 * find a rising zero crossing, like a cheap oscilliscope trigger.
	 */
	for(start=0; start < n - npts && samples[start] > 0; start++)
		;
	for(; start < n - npts && samples[start] < 0; start++)
		;

	for (i=0; i<npts; i++) {
		int y = center_y + samples[start+i]/scale;
		if (!first_wave && wave_point[i] == y)
			continue;
		write_row((Point){wave_r.min.x+i, y}, 1, &White);
		if (!first_wave)
			write_row((Point){wave_r.min.x+i, wave_point[i]}, 1, &Black);
		wave_point[i] = y;
	}
	first_wave = 0;
}

/*
 * Compute the color gradient for v, a value between 0 and 1.   Code shamelessly
 * stolen from GetColorGradient in audacity.
 */
float gradient[][3] = {
	{0.00, 0.00, 0.00},	// black
	{0.00, 0.00, 0.00},	// black
	{0.00, 0.00, 0.00},	// black
	{0.00, 0.00, 0.00},	// black
	{0.00, 0.00, 0.00},	// black	{0.01, 0.01, 0.01},	// mostly black
	{0.75, 0.75, 0.75},	// lt gray
	{0.30, 0.60, 1.00},	// lt blue
	{0.90, 0.10, 0.90},	// violet
	{1.00, 0.00, 0.00},	// red
	{1.00, 1.00, 1.00}	// white
};
#define	GSTEPS	((sizeof(gradient)/(sizeof(float)*3))-1)

Pixel
color_gradient(float v) {
	int left = v * GSTEPS;
	int right = (left == GSTEPS) ? GSTEPS: left + 1;
	float rweight = (v * GSTEPS) - left;
	float lweight = 1.0 - rweight;
	float r = (gradient[left][0] * lweight) + (gradient[right][0] * rweight);
	float g = (gradient[left][1] * lweight) + (gradient[right][1] * rweight);
	float b = (gradient[left][2] * lweight) + (gradient[right][2] * rweight);
	return SETRGB(r*Z, g*Z, b*Z);
}


//#define SHIFT	2	// how many pixels per sample display
#define SHIFT	1	// how many pixels per sample display
#define FFT_MAX	12000.0

// #define	DB(x)	(10*log10(x))
#define	DB(x)	(log1p(x))
#define PMAX 27.0		// just a guess

float ave_min_power = 5.0;

void
add_data(sample *samples, int n) {
	int x, y;
	char msgbuf[100];
	int i;
	float ave_power;
	double power_spectrum[(FFT_SIZE/2) + 1];	// actually, does "out" have this?

	sample vol_min = 0;
	sample vol_max = 0;
	int volume_ysize = DY(volume_r);
	int zero_vol_y = volume_r.min.y + volume_ysize/2;
	int vmax_y, vmin_y;
	int scale_y = (SHRT_MAX - SHRT_MIN)/volume_ysize;

	double pmax = 0.0, pmin=100.0;
	int fft_xsize = DX(fft_r);
	size_t bytes_copy_x = sizeof(Pixel)*(DX(volume_r) - SHIFT);

assert(n <= FFT_SIZE);

	for (i=0; i<n; i++) {
		in[i] = samples[i];
		if (samples[i] > vol_max)
			vol_max = samples[i];
		if (samples[i] < vol_min)
			vol_min = samples[i];
	}

	vmax_y = zero_vol_y + vol_max/scale_y;
	vmin_y = zero_vol_y + vol_min/scale_y;
	for (y=volume_r.min.y; y<volume_r.max.y; y++) {
		Pixel color = Black;
		if (y == zero_vol_y)
			color = Red;
		else if (y >= vmin_y && y <= vmax_y)
			color = White;

		if (output_device == X_out || option) {
			Pixel buf[3];
			buf[0] = color;
			buf[1] = buf[2] = Black;
			write_row((Point){xout, y}, 3, buf);
		} else {
			memmove(&screen[y][volume_r.min.x],
				&screen[y][volume_r.min.x+SHIFT],
				bytes_copy_x);
			for (x=0; x<SHIFT; x++)
				screen[y][volume_r.max.x - 1 - x] = color;
			write_row((Point){volume_r.min.x, y}, DX(volume_r),
				&screen[y][volume_r.min.x]);
		}

	}

	/* see doc/fftw.info */
	fftw_execute(plan);

	// All of this power computation is now probably quite wrong

	power_spectrum[0] = DB(creal(out[0])*creal(out[0]));	/* DC component */
	for (i=1; i<(FFT_SIZE/2) + 1; i++)
		power_spectrum[i] = DB(creal(out[i])*creal(out[i]) +
			cimag(out[i])*cimag(out[i]));
	if (FFT_SIZE % 2 == 0)	/* if even */
		power_spectrum[FFT_SIZE/2] = DB(cimag(0) * cimag(0));

	ave_power = 0.0;
	for (i=0; i<FFT_SIZE/2+1; i++) {
		ave_power += power_spectrum[i];
		if (power_spectrum[i] > pmax)
			pmax = power_spectrum[i];
		if (power_spectrum[i] < pmin)
			pmin = power_spectrum[i];
	}
	ave_power /= FFT_SIZE/2 + 1;

	if (pmin > ave_min_power) {
		ave_min_power += 0.01;
		if (ave_min_power >= PMAX)
			ave_min_power = PMAX - 0.1;
	} else
		ave_min_power -= 0.01;

	if (ave_min_power < 0.0)
		ave_min_power = 0.0;

	snprintf(msgbuf, sizeof(msgbuf),
		 "power %6.1f to %6.1f, %6.1f ave %6.1f",
		 pmin, pmax, ave_min_power, ave_power);
	write_string(load_r, White, msgbuf);

	for (i=0; i<DY(fft_r); i++) {
		int y = fft_r.min.y + i;
		int j;
		double p;
		Pixel color;

assert(map_fft_y[i] >= 0 && map_fft_y[i] <= FFT_SIZE/2 + 1);
		p = power_spectrum[map_fft_y[i]];
		if (p > PMAX)
			p = PMAX;
		p -= ave_min_power;
		if (p < 0.0)
			p = 0.0;

		color = color_gradient(p/(PMAX - ave_min_power));

		if (output_device == X_out || option) {
			Pixel buf[3];
			buf[0] = color;
			buf[1] = buf[2] = Black;
			write_row((Point){xout, y}, 3, buf);
		} else {
			for (j=0; j<1; j++) {
				memmove(&screen[y+j][fft_r.min.x],
					&screen[y+j][fft_r.min.x+SHIFT],
					bytes_copy_x);
				for (x=0; x<SHIFT; x++)
					screen[y+j][fft_r.max.x - 1 - x] =
						color;
				write_row((Point){fft_r.min.x,y+j},
					  fft_xsize, &screen[y+j][fft_r.min.x]);
			}
		}
	}
	xout++;
	if (xout >= fft_r.max.x)
		xout = fft_r.min.x;
	draw_wave(samples, n);
	flush_screen();
}

/*
 * the display routines have called us during idle time.  We use
 * the next sample from either the microphone or a file.  Even if we
 * are reading from a file, we still read and discard the microphone
 * input, for timing.
 */
void
do_idle(void) {
	int len = 0;
	sample buf[AUD_SAMP_SIZE];

	if (refresh_screen) {
		draw_screen();
		refresh_screen = 0;
	}

	if (src) {
		int n = AUD_SAMP_SIZE;
	
		if (frame + n > nframes)
			n = nframes - frame;
		if (n == 0) {
			select_mic();	// back to microphone
		} else {
			len = afReadFrames (src, AF_DEFAULT_TRACK,
			    buf, AUD_SAMP_SIZE);
			assert(len >= 0 && len <= AUD_SAMP_SIZE);
		}
		frame += n;
	}
	if (src) {
		sample dummybuf[AUD_SAMP_SIZE];
		(void) read_mic_samples(dummybuf, AUD_SAMP_SIZE);
	} else
		len = read_mic_samples(buf, AUD_SAMP_SIZE);
	
	if (src)
		write(dsp_fd, buf, len*sizeof(sample));
				queued += len*1000/AUDIO_RATE;

	if (len > 0) {
		add_data(buf, len);
	}
	show_load();
	flush_screen();
}

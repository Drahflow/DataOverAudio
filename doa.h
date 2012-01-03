#ifndef __DOA_H__
#define __DOA_H__

#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fftw3.h>
#include <errno.h>
#include <string.h>

#define DISP_VOLS 1
#define DISP_BITS 2
#define DISP_STATUS 4
#define DISP_INPUT 8
#define DISP_MOVEMENT 16

extern int dsp;
extern int speed;
extern int freqcount;
extern int multiple;
extern int fftwsize;
extern double trigger;

extern fftw_complex *in, *out;
extern fftw_plan plan, unplan;

struct s_tone {
	double freq, amp, vol;
	int pos;
};
extern s_tone tones[];
extern int tonenum;

extern int display;

double hz2pos(int);
int hz2pos(double);
void fillin(void);
void clearin(void);
void pushout(void);
void usetones(int);
void calcvols(void);
int tr(int);
void str(int);

#endif

#include "doa.h"
#include "doa_algo.h"

const char *algo = "s1d1";
const char *dspfilename = "/dev/dsp";
int dsp;
int speed = 44100;
int multiple = 5;
double trigger = 128000;
double volume = 1;

int display = 0;

int fftwsize = 4096;

int scantime = 60;
int freqcount = 10;

fftw_complex *in, *out;
fftw_plan plan, unplan;

s_tone tones[4096];
int tonenum = 0;
double ampsum = 0;

double pos2hz(int pos)
{
	return (double)pos * (double)speed / (double)fftwsize;
}

int hz2pos(double hz)
{
	return (int)(hz * fftwsize / speed);
}

double getvol(fftw_complex c) {
	return fabs(c[0]) + fabs(c[1]);
}

void usetones(int num)
{
	if(num > tonenum) {
		fprintf(stderr, "the algorithm needs %d tones\n", num);
		throw "you specified too few tones, use some more";
	}
	ampsum = 0;
	for(int i = 0; i < num; i++)
		ampsum += tones[i].amp;
}

void calcvols(void)
{
	if(display & DISP_VOLS) {
		printf("\n");
	}
	for(int i = 0; i < tonenum; i++) {
		tones[i].vol = getvol(out[tones[i].pos]) * tones[i].amp;
		if(display & DISP_VOLS) {
			const char *color;
			if     (tones[i].vol > trigger      ) color = "1;31";
			else if(tones[i].vol > trigger /   2) color = "0;31";
			else if(tones[i].vol > trigger /   4) color = "1;33";
			else if(tones[i].vol > trigger /   8) color = "1;32";
			else if(tones[i].vol > trigger /  16) color = "0;32";
			else if(tones[i].vol > trigger /  32) color = "1;36";
			else if(tones[i].vol > trigger /  64) color = "0;36";
			else if(tones[i].vol > trigger / 128) color = "1;34";
			else if(tones[i].vol > trigger / 256) color = "0;34";
			else if(tones[i].vol > trigger / 512) color = "1;37";
			else if(tones[i].vol > trigger /1024) color = "0;37";
			else if(tones[i].vol > trigger /2048) color = "1;30";
			else                                  color = "0;30";
			printf("\e[%sm#\e[0m", color);
		}
	}
	if(display & DISP_VOLS) {
		printf("  ");
	}
}

int tr(int t)
{
	return tones[t].vol > trigger;
}

void str(int t)
{
	in[tones[t].pos][0] = tones[t].amp;
}

void parseopts(int argc, char *argv[])
{
	for(int i = 2; i < argc && argv[i]; i++) {
		int argok = 0;
		if(argv[i+1]) {
			if(!strcmp(argv[i], "--dsp")) {
				dspfilename = argv[++i];
				argok = 1;
			}
			if(!strcmp(argv[i], "--speed")) {
				sscanf(argv[++i], "%d", &speed);
				argok = 1;
			}
			if(!strcmp(argv[i], "--fftwsize")) {
				sscanf(argv[++i], "%d", &fftwsize);
				argok = 1;
			}
			if(!strcmp(argv[i], "--scantime")) {
				sscanf(argv[++i], "%d", &scantime);
				argok = 1;
			}
			if(!strcmp(argv[i], "--freqcount")) {
				sscanf(argv[++i], "%d", &freqcount);
				argok = 1;
			}
			if(!strcmp(argv[i], "--vol")) {
				sscanf(argv[++i], "%lf", &volume);
				argok = 1;
			}
			if(!strcmp(argv[i], "--freq")) {
				tones[tonenum].amp = 1;
				sscanf(argv[++i], "%lf:%lf", &tones[tonenum].freq, &tones[tonenum].amp);
				tones[tonenum].pos = hz2pos(tones[tonenum].freq);
				tonenum++;
				argok = 1;
			}
			if(!strcmp(argv[i], "--freqlist")) {
				int num;
				double start, step;
				sscanf(argv[++i], "%d:%lf:%lf", &num, &start, &step);
				for(int i = 0; i < num; i++) {
					tones[tonenum].amp = 1;
					tones[tonenum].freq = start;
					tones[tonenum].pos = hz2pos(tones[tonenum].freq);
					tonenum++;
					start += step;
				}
				argok = 1;
			}
			if(!strcmp(argv[i], "--algo")) {
				algo = argv[++i];
				argok = 1;
			}
			if(!strcmp(argv[i], "--disp")) {
				sscanf(argv[++i], "%d", &display);
				argok = 1;
			}
			if(!strcmp(argv[i], "--multi")) {
				sscanf(argv[++i], "%d", &multiple);
				argok = 1;
			}
			if(!strcmp(argv[i], "--trigger")) {
				sscanf(argv[++i], "%lf", &trigger);
				argok = 1;
			}
		}
		if(!argok)
			throw "invalid argument";
	}
}

void initdsp(int mode)
{
	if((dsp = open(dspfilename, mode)) < 0)
		throw "cannot open dsp";
	int io = AFMT_S16_LE;
	if(ioctl(dsp, SNDCTL_DSP_SETFMT, &io) < 0)
		throw "could not change dsp format";
	if(ioctl(dsp, SNDCTL_DSP_SPEED, &speed) < 0)
		throw "could not set dsp speed";
}

void initfftw(void)
{
	in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * fftwsize);
	out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * fftwsize);
	plan = fftw_plan_dft_1d(fftwsize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	unplan = fftw_plan_dft_1d(fftwsize, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
	if(display & DISP_STATUS) {
		fprintf(stderr, "initialized\n");
	}
}
	
void help(int, char *[])
{
	printf("usage: ./doa method options\n");
	printf("\tmethod is one of --scan/-S, --send/-s and --recv/-r\n");
	printf("\toptions take the form --name value\n\n");
	printf("general options\n");
	printf("\t--dsp filename               --dsp /dev/dsp\n");
	printf("\t--speed hz                   --speed 44100\n");
	printf("\t--fftwsize bytes             --fftwsize 4096\n");
	printf("\t--algo algorithm             --algo s3rd3r\n");
	printf("\t--disp bitmask               --disp 15\n");
	printf("--scan specific\n");
	printf("\t--scantime secs              --scantime 10\n");
	printf("\t--freqcount num              --freqcount 9\n");
	printf("--send specific\n");
	printf("\t--vol factor                 --vol 0.2\n");
	printf("\t--multi num                  --multi 5\n");
	printf("--recv specific\n");
	printf("\t--trigger volume             --trigger 300000\n");
	printf("frequency options\n");
	printf("\t--freq hz[:amplification]    --freq 440:2\n");
	printf("\t--freqlist count:hz:deltahz  --freqlist 32:880:50\n");
	printf("possible algorithms\n");
	printf("\terror flags:\n");
	printf("\t\ta     measure amplification at start of transmission\n");
	printf("\t\tC     automatic data correction by freqency-redundancy\n");
	printf("\t\ts     automatic sync correction by freqency-redundancy\n");
	printf("\t\tS     continuous fast-sync measurement\n");
	printf("\tname         data bits   tones\n");
	printf("\t|||||||||||| |||| error  |||||\n");
	printf("\t|||||||||||| |||| |||||| |||||\n");
	printf("\ts1d1            1            2\n");
	printf("\ts3r1d3r1        1    sC      6\n");
	printf("\ts9r1d9r1        1    sC     18\n");
	printf("\ts1d1f           1    S       2\n");
	printf("\ts1d8            8            9\n");
	printf("\ts1d8a           8      a     9\n");
	printf("\ts1d8f           8    S       9\n");
	printf("\ts1d8fa          8    S a     9\n");
	printf("\ts1d4a8fa        8    S a     5\n");
	printf("\ts1d16          16           17\n");
	printf("\ts1d16a         16      a    17\n");
	printf("\ts1d16fa        16    S a    17\n");
	printf("\ts1d24fa        24    S a    25\n");
	printf("\ts1d32          32           33\n");
	printf("\ts1d32a         32      a    33\n");
	printf("\ts1d32fa        32    S a    33\n");
	printf("\ts1d64a         64      a    65\n");
	printf("\ts1d64fa        64    S a    65\n");
	printf("\ts1d128a       128      a   129\n");
	printf("\ts1d128fa      128    S a   129\n");
	printf("\tmorse           ?            1\n");
	printf("\tmorse-speaker   ?            1\n");
	exit(0);
}

void fillin(void)
{
	int16_t val;
	for(int i = 0; i < fftwsize; i++) {
		read(dsp, &val, 2);
		in[i][0] = (double)val;
		in[i][1] = 0;
	}
}

void pushout(void)
{
	int16_t val;
	for(int m = 0; m < multiple; m++) {
		for(int i = 0; i < fftwsize; i++) {
			val = (int16_t)(volume / ampsum * 32767 * out[i][0]);
			write(dsp, &val, 2);
		}
	}
}

void clearin(void)
{
	for(int i = 0; i < fftwsize; i++) {
		in[i][0] = 0;
		in[i][1] = 0;
	}
}

void scan(int argc, char *argv[])
{
	parseopts(argc, argv);
	initdsp(O_RDONLY); initfftw();

	double *vols = (double *)malloc(fftwsize * sizeof(double));
	for(int i = 0; i < fftwsize; i++) {
		vols[i] = 0;
	}
	
	printf("scanning for %d seconds...\n", scantime);
	for(int i = 0; i < speed * scantime / fftwsize; i++) {
		fillin(); fftw_execute(plan);
		for(int j = 0; j < fftwsize; j++) {
			vols[j] += getvol(out[j]);
		}
	}

	double minvol;
	int minpos = -1;
	
	for(int i = 0; i < freqcount; i++) {
		if(minpos != -1) 
			for(int k = minpos - 15; k < minpos + 15; k++)
				vols[k] = 1E64;
		minvol = 1E64;
		minpos = -1;
		for(int j = 10; j < fftwsize / 4; j++) {
			if(vols[j] < minvol) {
				minvol = vols[j];
				minpos = j;
			}
		}
		if(minpos == -1) {
			printf("you got quite a problem, there are no free frequencies\n");
			break;
		} else {
			if(display & DISP_STATUS) {
				printf("%8.2lf Hz @ %08.2lf\n", pos2hz(minpos), vols[minpos]);
			} else {
				printf(" --freq %8.0lf", pos2hz(minpos));
			}
		}
	}
	if(!(display & DISP_STATUS))
		printf("\n");
	printf("trigger should be: %032.0lf\n", 8 * vols[minpos] * fftwsize / speed / scantime);
}

void recv(int argc, char *argv[])
{
	parseopts(argc, argv);
	initdsp(O_RDONLY); initfftw();
	if(!strcmp(algo, "s1d1")) s1d1_recv();
	else if(!strcmp(algo, "s1d8")) s1dx8_recv(1);
	else if(!strcmp(algo, "s1d8a")) s1dx8a_recv(1);
	else if(!strcmp(algo, "s1d16")) s1dx8_recv(2);
	else if(!strcmp(algo, "s1d16a")) s1dx8a_recv(2);
	else if(!strcmp(algo, "s1d32")) s1dx8_recv(4);
	else if(!strcmp(algo, "s1d32a")) s1dx8a_recv(4);
	else if(!strcmp(algo, "s1d64a")) s1dx8a_recv(8);
	else if(!strcmp(algo, "s1d128a")) s1dx8a_recv(16);
	else if(!strcmp(algo, "s3r1d3r1")) sxr1dxr1_recv(3);
	else if(!strcmp(algo, "s9r1d9r1")) sxr1dxr1_recv(9);
	else if(!strcmp(algo, "s1d1f")) s1d1f_recv();
	else if(!strcmp(algo, "s1d8f")) s1dx8f_recv(1);
	else if(!strcmp(algo, "s1d8fa")) s1dx8fa_recv(1);
	else if(!strcmp(algo, "s1d16fa")) s1dx8fa_recv(2);
	else if(!strcmp(algo, "s1d24fa")) s1dx8fa_recv(3);
	else if(!strcmp(algo, "s1d32fa")) s1dx8fa_recv(4);
	else if(!strcmp(algo, "s1d64fa")) s1dx8fa_recv(8);
	else if(!strcmp(algo, "s1d128fa")) s1dx8fa_recv(16);
	else if(!strcmp(algo, "s1d4a8fa")) s1dx4ax8fa_recv(1);
	else if(!strcmp(algo, "morse")) morse_recv();
	else if(!strcmp(algo, "morse-speaker")) morse_recv();
	else throw "invalid algorithm";
}

void send(int argc, char *argv[])
{
	parseopts(argc, argv);
	initdsp(O_WRONLY); initfftw();
	if(!strcmp(algo, "s1d1")) s1d1_send();
	else if(!strcmp(algo, "s1d8")) s1dx8_send(1);
	else if(!strcmp(algo, "s1d8a")) s1dx8a_send(1);
	else if(!strcmp(algo, "s1d16")) s1dx8_send(2);
	else if(!strcmp(algo, "s1d16a")) s1dx8a_send(2);
	else if(!strcmp(algo, "s1d32")) s1dx8_send(4);
	else if(!strcmp(algo, "s1d32a")) s1dx8a_send(4);
	else if(!strcmp(algo, "s1d64a")) s1dx8a_send(8);
	else if(!strcmp(algo, "s1d128a")) s1dx8a_send(16);
	else if(!strcmp(algo, "s3r1d3r1")) sxr1dxr1_send(3);
	else if(!strcmp(algo, "s9r1d9r1")) sxr1dxr1_send(9);
	else if(!strcmp(algo, "s1d1f")) s1d1f_send();
	else if(!strcmp(algo, "s1d8f")) s1dx8f_send(1);
	else if(!strcmp(algo, "s1d8fa")) s1dx8fa_send(1);
	else if(!strcmp(algo, "s1d16fa")) s1dx8fa_send(2);
	else if(!strcmp(algo, "s1d24fa")) s1dx8fa_send(3);
	else if(!strcmp(algo, "s1d32fa")) s1dx8fa_send(4);
	else if(!strcmp(algo, "s1d64fa")) s1dx8fa_send(8);
	else if(!strcmp(algo, "s1d128fa")) s1dx8fa_send(16);
	else if(!strcmp(algo, "s1d4a8fa")) s1dx4ax8fa_send(1);
	else if(!strcmp(algo, "morse")) morse_send();
	else if(!strcmp(algo, "morse-speaker")) morse_speaker_send();
	else throw "invalid algorithm";
}

int main(int argc, char *argv[])
{
	try {
		if(argc > 1) {
			if(!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) help(argc, argv);
			if(!strcmp(argv[1], "--scan") || !strcmp(argv[1], "-S")) scan(argc, argv);
			if(!strcmp(argv[1], "--recv") || !strcmp(argv[1], "-r")) recv(argc, argv);
			if(!strcmp(argv[1], "--send") || !strcmp(argv[1], "-s")) send(argc, argv);
		} else {
			help(0, NULL);
		}
	} catch(const char *err) {
		if(errno) {
			fprintf(stderr, "error: %s: %s\n", err, strerror(errno));
		} else {
			fprintf(stderr, "error: %s\n", err);
		}
		exit(1);
	}
	return 0;
}

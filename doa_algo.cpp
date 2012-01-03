#include "doa.h"
#include "doa_algo.h"

/*
 * return:
 * 0: nothing interesting
 * 1: init
 * 2: measure now
 */
int processsync(int *state, int sync)
{
	if(*state == 0 && sync) {
		*state = 4;
		return 1;
	}
	if(*state == 0) return 0;
	if(*state > 0 && !sync) {
		*state = -4;
	} else if(*state < 0 && sync) {
		*state = 4;
	}
	if(*state > 1) (*state)--;
	if(*state < -1) (*state)++;
	if(abs(*state) == 2) return 2;
	return 0;
}

static int16_t *data = NULL;
static int movement;
int findfastsync(int *sync)
{
	int best;
	double bestval;
	int i, j;

	if(!data) {
		data = (int16_t *)malloc(fftwsize * 3 * sizeof(*data));
		for(i = 0; i < 3 * fftwsize; i++) {
			read(dsp, &data[i], 2); 
		}
		movement = fftwsize + fftwsize / 2;
		if(display & DISP_STATUS) {
			printf("data space allocated\n");
			fflush(stdout);
		}
	} else {
		movement -= fftwsize / 2;
		if(display & DISP_MOVEMENT) {
			printf("movement %d\n", movement);
			fflush(stdout);
		}
		for(i = movement; i < 3 * fftwsize; i++) {
			data[i - movement] = data[i];
		}
	}
	for(i = 3 * fftwsize - movement; i < 3 * fftwsize; i++) {
		read(dsp, &data[i], 2); 
	}
	best = -1;
	bestval = trigger;
	for(i = 0; i < 2 * fftwsize; i += fftwsize / 16) {
		for(j = 0; j < fftwsize; j++) {
			in[j][0] = (double)(data[i + j]);
			in[j][1] = 0;
		}
		fftw_execute(plan); calcvols();
		if(*sync && tones[0].vol < bestval) {
			bestval = tones[0].vol;
			best = i;
		}
		if(!*sync && tones[0].vol > bestval) {
			bestval = tones[0].vol;
			best = i;
		}
	}
	if(best == -1) {
		if(display & DISP_STATUS) {
			printf("sync miss\n");
			fflush(stdout);
		}
		movement = fftwsize + fftwsize / 2;
		return 0;
	}

	*sync = !*sync;
	movement = best + fftwsize;

	for(j = 0; j < fftwsize; j++) {
		in[j][0] = (double)(data[best + j]);
		in[j][1] = 0;
	}
	fftw_execute(plan); calcvols();
	return 2;
}

void s1d1_recv(void)
{
	usetones(2);
	int bitpos;
	int sstate = 0;
	int data;

	while(1) {
		fillin(); fftw_execute(plan); calcvols();
		switch(processsync(&sstate, tr(0))) {
			case 1:
				bitpos = 0;
				data = 0;
				break;
			case 2:
				if(tr(1)) {
					data |= 1 << bitpos;
					if(display & DISP_BITS)
						printf("1");
				} else {
					data &= ~(1 << bitpos);
					if(display & DISP_BITS)
						printf("0");
				}
				if(display & DISP_BITS)
					printf(" @ %d", bitpos);
				bitpos++;
				if(bitpos >= 8) {
					printf("%c", data);
					fflush(stdout);
					bitpos = 0;
				}
				break;
		}
	}
}

void s1d1_send(void)
{
	usetones(2);
	int data;

	while(data != EOF) {
		data = getchar();
		if(display & DISP_INPUT) {
			printf("%c", data);
			fflush(stdout);
		}
		for(int bitpos = 0; bitpos < 8; bitpos++) {
			clearin();

			if(!(bitpos % 2)) str(0);
			if(data & (1 << bitpos)) str(1);

			fftw_execute(unplan); pushout();
		}
	}
}

void s1dx8_recv(int bytenum)
{
	usetones(1 + 8 * bytenum);
	int bitpos;
	int sstate = 0;
	int data;

	while(1) {
		fillin(); fftw_execute(plan); calcvols();
		switch(processsync(&sstate, tr(0))) {
			case 1:
				break;
			case 2:
				for(int dpos = 0; dpos < bytenum; dpos++) {
					data = 0;
					for(bitpos = 0; bitpos < 8; bitpos++) {
						if(tr(1 + bitpos + 8 * dpos)) {
							data |= (1 << bitpos);
							if(display & DISP_BITS)
								printf("0");
						} else {
							if(display & DISP_BITS)
								printf("1");
						}
					}
					if(display & DISP_BITS)
						printf(" @ %d-%d  ", 8 * dpos, 8 * dpos + 7);
					printf("%c", data);
				}
				fflush(stdout);
				break;
		}
	}
}

void s1dx8_send(int bytenum)
{
	usetones(1 + 8 * bytenum);
	int data;
	int sstate = 1;
	int done = 0;

	while(!done) {
		clearin();

		if(sstate) str(0);
		sstate = !sstate;

		for(int dpos = 0; dpos < bytenum && !done; dpos++) {
			if((data = getchar()) == EOF) done = 1;
			if(display & DISP_INPUT) {
				printf("%c", data);
				fflush(stdout);
			}
			for(int bitpos = 0; bitpos < 8; bitpos++) {
				if(data & (1 << bitpos)) str(1 + bitpos + 8 * dpos);
			}
		}

		fftw_execute(unplan); pushout();
	}
}

void s1dx8a_recv(int bytenum)
{
	usetones(1 + 8 * bytenum);
	int sstate = 0;
	double onvol, offvol;

	if(display & DISP_STATUS) {
		printf("receiving adaption data\n");
	}
	for(int i = 0; i < 8 * bytenum + 1; i++) {
		sstate = 0;
		while(sstate != -1) {
			fillin(); fftw_execute(plan); calcvols();
			switch(processsync(&sstate, tr(0))) {
				case 1:
					break;
				case 2:
					if(sstate > 0) {
						onvol = tones[i].vol;
					} else {
						offvol = tones[i].vol;
						tones[i].amp = (trigger + 0.4 * (onvol - offvol)) / onvol;
						if(display & DISP_STATUS) {
							printf("amp set to %lf\n", tones[i].amp);
						}
					}
					break;
			}
		}
	}
	if(display & DISP_STATUS) {
		printf("adaption data received\n");
	}

	s1dx8_recv(bytenum);
}

void s1dx8a_send(int bytenum)
{
	usetones(1 + 8 * bytenum);

	if(display & DISP_STATUS) {
		printf("sending adaption data\n");
		fflush(stdout);
	}
	for(int i = 0; i < 8 * bytenum + 1; i++) {
		clearin();
		str(0);
		str(i);
		fftw_execute(unplan); pushout();
		clearin();
		fftw_execute(unplan); pushout();
	}
	if(display & DISP_STATUS) {
		printf("adaption sent\n");
		fflush(stdout);
	}

	s1dx8_send(bytenum);
}

void sxr1dxr1_recv(int red)
{
	usetones(2 * red);
	int bitpos;
	int sstate = 0;
	int data;
	int bit;

	while(1) {
		fillin(); fftw_execute(plan); calcvols();
		bit = 0;
		for(int i = 0; i < red; i++) {
			if(tr(i)) bit++;
		}
		switch(processsync(&sstate, bit > (red / 2))) {
			case 1:
				bitpos = 0;
				data = 0;
				break;
			case 2:
				bit = 0;
				for(int i = 0; i < red; i++) {
					if(tr(red + i)) bit++;
				}
				if(bit > (red / 2)) {
					data |= 1 << bitpos;
					if(display & DISP_BITS)
						printf("1");
				} else {
					data &= ~(1 << bitpos);
					if(display & DISP_BITS)
						printf("0");
				}
				if(display & DISP_BITS)
					printf(" @ %d", bitpos);
				bitpos++;
				if(bitpos >= 8) {
					printf("%c", data);
					fflush(stdout);
					bitpos = 0;
				}
				break;
		}
	}
}

void sxr1dxr1_send(int red)
{
	usetones(2 * red);
	int data;

	while(data != EOF) {
		data = getchar();
		if(display & DISP_INPUT) {
			printf("%c", data);
			fflush(stdout);
		}
		for(int bitpos = 0; bitpos < 8; bitpos++) {
			clearin();

			if(!(bitpos % 2)) {
				for(int i = 0; i < red; i++) {
					str(i);
				}
			}
			if(data & (1 << bitpos)) {
				for(int i = 0; i < red; i++) {
					str(i + red);
				}
			}

			fftw_execute(unplan); pushout();
		}
	}
}

void s1d1f_recv(void)
{
	usetones(2);
	int bitpos;
	int sstate = 0;
	int data;

	multiple = 1;
	bitpos = 0;
	data = 0;
	while(1) {
		switch(findfastsync(&sstate)) {
			case 2:
				if(tr(1)) {
					data |= 1 << bitpos;
					if(display & DISP_BITS)
						printf("1");
				} else {
					data &= ~(1 << bitpos);
					if(display & DISP_BITS)
						printf("0");
				}
				if(display & DISP_BITS)
					printf(" @ %d", bitpos);
				bitpos++;
				if(bitpos >= 8) {
					printf("%c", data);
					fflush(stdout);
					bitpos = 0;
				}
				break;
		}
	}
}

void s1d1f_send(void)
{
	multiple = 1;
	s1d1_send();
}

void s1dx8f_recv(int bytenum)
{
	usetones(1 + 8 * bytenum);
	int bitpos;
	int sstate = 0;
	int data;

	multiple = 1;
	bitpos = 0;
	data = 0;
	while(1) {
		switch(findfastsync(&sstate)) {
			case 2:
				for(int dpos = 0; dpos < bytenum; dpos++) {
					data = 0;
					for(bitpos = 0; bitpos < 8; bitpos++) {
						if(tr(1 + bitpos + 8 * dpos)) {
							data |= (1 << bitpos);
							if(display & DISP_BITS)
								printf("0");
						} else {
							if(display & DISP_BITS)
								printf("1");
						}
					}
					if(display & DISP_BITS)
						printf(" @ %d-%d  ", 8 * dpos, 8 * dpos + 7);
					printf("%c", data);
				}
				fflush(stdout);
				break;
		}
	}
}

void s1dx8f_send(int bytenum)
{
	multiple = 1;
	s1dx8_send(bytenum);
}

void s1dx8fa_recv(int bytenum)
{
	usetones(1 + 8 * bytenum);
	int sstate = 0;
	double onvol, offvol;

	if(display & DISP_STATUS) {
		printf("receiving adaption data\n");
	}
	for(int i = 0; i < 8 * bytenum + 1; i++) {
		while(findfastsync(&sstate) != 2);
		onvol = tones[i].vol;
		while(findfastsync(&sstate) != 2);
		offvol = tones[i].vol;
		tones[i].amp = (trigger + 0.4 * (onvol - offvol)) / onvol;
		if(display & DISP_STATUS) {
			printf("amp set to %lf\n", tones[i].amp);
		}
	}
	if(display & DISP_STATUS) {
		printf("adaption data received\n");
	}

	s1dx8f_recv(bytenum);
}

void s1dx8fa_send(int bytenum)
{
	multiple = 1;
	s1dx8a_send(bytenum);
}

void s1dx4ax8fa_recv(int bytenum)
{
	usetones(1 + 4 * bytenum);
	int sstate = 0;
	double onvol, offvol;
	int bitpos;
	int data;

	if(display & DISP_STATUS) {
		printf("receiving adaption data\n");
		fflush(stdout);
	}
	for(int i = 0; i < 4 * bytenum + 1; i++) {
		while(findfastsync(&sstate) != 2);
		onvol = tones[i].vol;
		while(findfastsync(&sstate) != 2);
		offvol = tones[i].vol;
		if(i == 0) {
			tones[i].amp = (trigger + 0.4 * (onvol - offvol)) / onvol;
		} else {
			tones[i].amp = trigger / onvol;
		}
		if(display & DISP_STATUS) {
			printf("amp set to %lf\n", tones[i].amp);
			fflush(stdout);
		}
	}
	if(display & DISP_STATUS) {
		printf("adaption data received\n");
		fflush(stdout);
	}

	multiple = 1;
	bitpos = 0;
	data = 0;
	while(1) {
		switch(findfastsync(&sstate)) {
			case 2:
				for(int dpos = 0; dpos < bytenum; dpos++) {
					data = 0;
					for(bitpos = 0; bitpos < 8; bitpos += 2) {
						int tpos = 4 * dpos + bitpos / 2 + 1;
						if(tones[tpos].vol >= trigger * 0.7) {
							data |= (3 << bitpos);
							if(display & DISP_BITS)
								printf("11");
						} else if(tones[tpos].vol >= trigger * 0.2) {
							data |= (2 << bitpos);
							if(display & DISP_BITS)
								printf("10");
						} else if(tones[tpos].vol >= trigger * 0.05) {
							data |= (1 << bitpos);
							if(display & DISP_BITS)
								printf("01");
						} else {
							data |= (0 << bitpos);
							if(display & DISP_BITS)
								printf("00");
						}
					}
					if(display & DISP_BITS)
						printf(" @ %d-%d  ", 8 * dpos, 8 * dpos + 7);
					printf("%c", data);
				}
				fflush(stdout);
				break;
		}
	}
}

void s1dx4ax8fa_send(int bytenum)
{
	usetones(1 + 4 * bytenum);
	multiple = 1;
	int data;
	int sstate = 1;
	int done = 0;

	if(display & DISP_STATUS) {
		printf("sending adaption data\n");
		fflush(stdout);
	}
	for(int i = 0; i < 4 * bytenum + 1; i++) {
		clearin();
		str(0);
		str(i);
		fftw_execute(unplan); pushout();
		clearin();
		fftw_execute(unplan); pushout();
	}
	if(display & DISP_STATUS) {
		printf("adaption sent\n");
		fflush(stdout);
	}

	while(!done) {
		clearin();

		if(sstate) str(0);
		sstate = !sstate;

		for(int dpos = 0; dpos < bytenum && !done; dpos++) {
			if((data = getchar()) == EOF) done = 1;
			if(display & DISP_INPUT) {
				printf("%c", data);
				fflush(stdout);
			}
			for(int bitpos = 0; bitpos < 8; bitpos += 2) {
				int tpos = 4 * dpos + bitpos / 2 + 1;
				if((data & (3 << bitpos)) == (3 << bitpos))
					in[tones[tpos].pos][0] = tones[tpos].amp;
				else if((data & (3 << bitpos)) == (2 << bitpos))
					in[tones[tpos].pos][0] = tones[tpos].amp * 0.25;
				else if((data & (3 << bitpos)) == (1 << bitpos))
					in[tones[tpos].pos][0] = tones[tpos].amp * 0.07;
				else
					in[tones[tpos].pos][0] = 0;
			}
		}

		fftw_execute(unplan); pushout();
	}
}

const char *morse_alpha[26] = {
	".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", // A-I
	".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", // J-R
	"...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." // S-Z
};

void morse_recv(void)
{
	usetones(1);
	int state = 0;
	int len = -1;
	char buf[32];
	int bufpos = 0;
	int oldlen;
	int data;

	while(1) {
		fillin(); fftw_execute(plan); calcvols();
		data = '\0';
		if(tr(0) && !state) {
			if(len <= multiple / 2) {
				len = oldlen + len;
			} else {
				if(len > 2 * multiple && bufpos) {
					buf[bufpos] = '\0';
					bufpos = 0;
					for(data = 0; data < 26; data++) {
						if(!strcmp(buf, morse_alpha[data])) break;
					}
					if(data < 26) data = data + 'A';
					else data = 'e';
					printf("%c", data);
					fflush(stdout);
				}
				if(len > 5 * multiple) {
					printf(" ");
					fflush(stdout);
				}
				oldlen = len;
				len = 0;
				state = 1;
			}
		} else if(!tr(0) && state) {
			if(len <= multiple / 2) {
				len = oldlen + len;
				bufpos--;
			} else {
				oldlen = len;
				if(len < multiple * 2) {
					if(display & DISP_BITS) {
						printf(".");
						fflush(stdout);
					}
					buf[bufpos++] = '.';
					oldlen = 0;
				} else {
					if(display & DISP_BITS) {
						printf("-");
						fflush(stdout);
					}
					buf[bufpos++] = '-';
					oldlen = 0;
				}
				len = 0;
				state = 0;
			}
		}
		if(len >= 0) len++;
		if(bufpos > 31) {
			printf("error: letter too long\n");
			fflush(stdout);
			bufpos = 0;
		}
	}
}

void morse_send(void)
{
	usetones(1);
	int data;
	int done = 0;
	const char *letter;

	while(!done) {
		clearin();

		if((data = getchar()) == EOF) done = 1;
		if(display & DISP_INPUT) {
			printf("%c", data);
			fflush(stdout);
		}

		letter = "";
		if(data >= 'A' && data <= 'Z') letter = morse_alpha[data - 'A'];
		if(data == ' ') letter = "    ";

		while(*letter) {
			if(*letter == '.') {
				clearin(); str(0); fftw_execute(unplan); pushout();
			}
			if(*letter == '-') {
				clearin(); str(0); fftw_execute(unplan);
				for(int i = 0; i < 3; i++) {
					pushout();
				}
			}
			clearin(); fftw_execute(unplan); pushout();
			letter++;
		}
		clearin(); fftw_execute(unplan); pushout(); pushout();

	}
}

void morse_speaker_send(void)
{
	usetones(1);
	int data;
	int done = 0;
	int fftwdur = 1000 * fftwsize / speed;
	const char *letter;

	while(!done) {
		if((data = getchar()) == EOF) done = 1;
		if(display & DISP_INPUT) {
			printf("%c", data);
			fflush(stdout);
		}

		letter = "";
		if(data >= 'A' && data <= 'Z') letter = morse_alpha[data - 'A'];
		if(data == ' ') letter = "    ";

		while(*letter) {
			if(*letter == '.') {
				printf("\e[10;%d]\e[11;%d]\a", (int)(tones[0].freq), fftwdur);
				fflush(stdout); usleep(fftwdur * 1000);
			}
			if(*letter == '-') {
				printf("\e[10;%d]\e[11;%d]\a", (int)(tones[0].freq), 3 * fftwdur);
				fflush(stdout); usleep(3 * fftwdur * 1000);
			}
			usleep(fftwdur * 1000);
			letter++;
		}
		usleep(2 * fftwdur * 1000);
	}
}

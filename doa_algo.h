#ifndef __DOA_ALGO_H__
#define __DOA_ALGO_H__

void s1d1_recv(void);
void s1d1_send(void);

void s1dx8_recv(int);
void s1dx8_send(int);

void s1dx8a_recv(int);
void s1dx8a_send(int);

void sxr1dxr1_recv(int);
void sxr1dxr1_send(int);

void s1d1f_recv();
void s1d1f_send();

void s1dx8f_recv(int);
void s1dx8f_send(int);

void s1dx8fa_recv(int);
void s1dx8fa_send(int);

void s1dx4ax8fa_recv(int);
void s1dx4ax8fa_send(int);

void morse_recv();
void morse_send();
void morse_speaker_send();

#endif

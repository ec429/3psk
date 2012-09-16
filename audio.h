#pragma once
/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	audio: AUDio Input/Output subsystem
*/

#include <stdint.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_audioin.h>

#include <unistd.h>
#ifdef WINDOWS
#define SLEEP	Sleep(5)
#else
#define SLEEP	usleep(5e3)
#endif

#include "bits.h"

#define SAMPLE_RATE	22050
#define AUDIOBUFLEN	1024

typedef struct
{
	unsigned int rp, wp;
	int16_t buf[AUDIOBUFLEN];
	unsigned int srate;
	bool underrun;
}
audiobuf;

int init_audiorx(audiobuf *a);
void rxaudio(void *udata, Uint8 *stream, int len);
void stop_audiorx(audiobuf *a);
int rxsample(audiobuf *a, int16_t *samp); // returns 0 if we got a sample

int init_audiotx(audiobuf *a);
void txaudio(void *udata, Uint8 *stream, int len);
void stop_audiotx(audiobuf *a);
void txsample(audiobuf *a, int16_t samp);
bool cantx(audiobuf *a);

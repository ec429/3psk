/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	audio: AUDio Input/Output subsystem
*/

#include "audio.h"
#ifdef WINDOWS
#include <windef.h>
#include <winbase.h>
#endif

int init_audiorx(audiobuf *a)
{
	if(SDL_InitAudioIn()<0)
	{
		fprintf(stderr, "Failed to initialise SDL_audioin: %s\n", SDL_GetError());
		return(1);
	}
	SDL_AudioSpec expected, result;
	expected.format=AUDIO_S16;
	expected.silence=0;
	expected.freq=SAMPLE_RATE;
	expected.channels=1;
	expected.samples=AUDIOBUFLEN;
	expected.callback=rxaudio;
	expected.userdata=a;
	for(unsigned int i=0;i<AUDIOBUFLEN;i++)
		a->buf[i]=0;
	a->rp=a->wp=0;

	if(SDL_OpenAudioIn(&expected,&result)<0)
	{
		fprintf(stderr, "Can't open audio input: %s\n", SDL_GetError());
		return(1);
	}
	a->srate=SAMPLE_RATE; // should use result.freq, but SDL_audioin doesn't populate it
	SDL_PauseAudioIn(0);
	return(0);
}

void rxaudio(void *udata, Uint8 *stream, int len)
{
	audiobuf *a=udata;
	Sint16 *sData=(Sint16 *)stream;
	for(int i=0;i<len>>1;i++)
	{
		unsigned int newwp=(a->wp+1)%AUDIOBUFLEN;
		while(a->rp==newwp)
		{
			a->underrun=false;
			SLEEP;
		}
		a->buf[newwp]=sData[i];
		a->wp=newwp;
	}
}

void stop_audiorx(audiobuf *a)
{
	a->rp=-1;
	SDL_CloseAudioIn();
	SDL_QuitAudioIn();
}

int rxsample(audiobuf *a, int16_t *samp)
{
	unsigned int newrp=(a->rp+1)%AUDIOBUFLEN;
	if(a->wp==newrp)
		return(1);
	if(samp)
		*samp=a->buf[newrp];
	a->rp=newrp;
	return(0);
}

int init_audiotx(audiobuf *a)
{
	if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0)
	{
		fprintf(stderr, "Failed to initialise SDL_audio: %s\n", SDL_GetError());
		return(1);
	}
	SDL_AudioSpec expected;
	expected.format=AUDIO_S16;
	expected.silence=0;
	expected.freq=SAMPLE_RATE;
	expected.channels=1;
	expected.samples=AUDIOBUFLEN;
	expected.callback=txaudio;
	expected.userdata=a;
	for(unsigned int i=0;i<AUDIOBUFLEN;i++)
		a->buf[i]=0;
	a->rp=a->wp=0;
	SDL_AudioSpec result;
	if(SDL_OpenAudio(&expected, &result)<0)
	{
		fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
		return(1);
	}
	if(result.format!=expected.format)
	{
		fprintf(stderr, "Failed to get the right audio format\n");
		return(1);
	}
	if(result.freq!=expected.freq)
	{
		fprintf(stderr, "Warning, got the wrong sample rate; strange things may happen\n");
		fprintf(stderr, "\tRequested %uHz, got %uHz\n", expected.freq, result.freq);
	}
	a->srate=result.freq;
	SDL_PauseAudio(0);
	return(0);
}

void txaudio(void *udata, Uint8 *stream, int len)
{
	audiobuf *a=udata;
	Sint16 *sData=(Sint16 *)stream;
	for(int i=0;i<len>>1;i++)
	{
		unsigned int newrp=(a->rp+1)%AUDIOBUFLEN;
		while(a->wp==newrp)
		{
			a->underrun=true;
			SLEEP;
		}
		sData[i]=a->buf[newrp];
		a->rp=newrp;
	}
}

void stop_audiotx(audiobuf *a)
{
	a->wp=-1;
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void txsample(audiobuf *a, int16_t samp)
{
	unsigned int newwp=(a->wp+1)%AUDIOBUFLEN;
	while(a->rp==newwp)
	{
		a->underrun=false;
		SLEEP;
	}
	a->buf[newwp]=samp;
	a->wp=newwp;
}

/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	audio: AUDio Input/Output subsystem
*/

#include "audio.h"
#include <math.h> // for floor()

int init_audiorx(audiobuf *a, unsigned int audiobuflen, unsigned int sdlbuflen, FILE *wav)
{
	SDL_AudioSpec expected, result;
	a->audiobuflen=audiobuflen;
	if(wav)
	{
#ifdef WAVLIB
		a->wav=wav;
		int e=read_wh44(wav, &a->wavhdr);
		if(e) return(e);
#else /* !WAVLIB */
		fprintf(stderr, "WAV support (--rx) not compiled in!\n");
		return(1);
#endif
	}
	else
	{
		a->wav=NULL;
	}
	if(SDL_InitAudioIn()<0)
	{
		fprintf(stderr, "Failed to initialise SDL_audioin: %s\n", SDL_GetError());
		return(1);
	}
	expected.format=AUDIO_S16;
	expected.silence=0;
	expected.freq=SAMPLE_RATE;
	expected.channels=1;
	expected.samples=sdlbuflen;
	expected.callback=rxaudio;
	expected.userdata=a;
	a->buf=malloc(a->audiobuflen*sizeof(int16_t));
	if(!a->buf)
	{
		perror("malloc");
		return(1);
	}
	for(unsigned int i=0;i<a->audiobuflen;i++)
		a->buf[i]=0;
	a->rp=a->wp=0;

	if(wav)
	{
#ifdef WAVLIB
		a->srate=a->wavhdr.sample_rate;
#endif
	}
	else
	{
		a->srate=SAMPLE_RATE; // should use result.freq, but SDL_audioin doesn't populate it
	}
	if(SDL_OpenAudioIn(&expected,&result)<0)
	{
		fprintf(stderr, "Can't open audio input: %s\n", SDL_GetError());
		return(1);
	}
	SDL_PauseAudioIn(0);
	return(0);
}

void rxaudio(void *udata, Uint8 *stream, int len)
{
	audiobuf *a=udata;
	Sint16 *sData=(Sint16 *)stream;
	for(int i=0;i<len>>1;i++)
	{
		unsigned int newwp=(a->wp+1)%a->audiobuflen;
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
	if(a->wav)
	{
#ifdef WAVLIB
		if(feof(a->wav))
		{
			fclose(a->wav);
			a->wav=NULL;
		}
		else
		{
			double v=read_sample(a->wavhdr, a->wav);
			int16_t i=floor((v-0.5)*65535);
			if(samp) *samp=i;
			return(0);
		}
#else
		fprintf(stderr, "a->wav was set for some reason but no WAVLIB, so unsetting\n");
		a->wav=NULL;
#endif
	}
	unsigned int newrp=(a->rp+1)%a->audiobuflen;
	if(a->wp==newrp)
	{
		a->underrun=true;
		return(1);
	}
	if(samp)
		*samp=a->buf[newrp];
	a->rp=newrp;
	return(0);
}

int init_audiotx(audiobuf *a, unsigned int audiobuflen, unsigned int sdlbuflen)
{
	if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0)
	{
		fprintf(stderr, "Failed to initialise SDL_audio: %s\n", SDL_GetError());
		return(1);
	}
	a->audiobuflen=audiobuflen;
	SDL_AudioSpec expected;
	expected.format=AUDIO_S16;
	expected.silence=0;
	expected.freq=SAMPLE_RATE;
	expected.channels=1;
	expected.samples=sdlbuflen;
	expected.callback=txaudio;
	expected.userdata=a;
	a->buf=malloc(a->audiobuflen*sizeof(int16_t));
	if(!a->buf)
	{
		perror("malloc");
		return(1);
	}
	for(unsigned int i=0;i<a->audiobuflen;i++)
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
		unsigned int newrp=(a->rp+1)%a->audiobuflen;
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
	unsigned int newwp=(a->wp+1)%a->audiobuflen;
	while(a->rp==newwp)
	{
		a->underrun=false;
		SLEEP;
	}
	a->buf[newwp]=samp;
	a->wp=newwp;
}

bool cantx(audiobuf *a)
{
	bool can=a->rp!=((a->wp+1)%a->audiobuflen);
	if(!can) a->underrun=false;
	return(can);
}

/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
*/

#include <stdio.h>
//#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include "atg/atg.h"
#include "wav.h"
#include "bits.h"
#include "varicode.h"

#define CONSLEN	32
#define BITBUFLEN	16
#define PHASLEN	50

#define INLINELEN	80
#define OUTLINELEN	80
#define OUTLINES	6

#define CONS_BG	(atg_colour){31, 31, 15, ATG_ALPHA_OPAQUE}
#define PHAS_BG	(atg_colour){15, 31, 31, ATG_ALPHA_OPAQUE}

int pset(SDL_Surface *s, unsigned int x, unsigned int y, atg_colour c);
int line(SDL_Surface *s, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, atg_colour c);
void ztoxy(fftw_complex z, double gsf, int *x, int *y);
void addchar(char *intextleft, char *intextright, char what);

int main(int argc, char **argv)
{
	double bw=150; // IF bandwidth, Hz
	unsigned long blklen;
	bool last=false;
	double centre=440; // centre frequency, Hz
	double aif=3000; // approximate IF, Hz
	double slow=4.0;
	double am=1.2;
	bool moni=false, afc=false;
	unsigned int txbaud=60;
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--bw=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &bw);
			last=false;
		}
		else if(strncmp(argv[arg], "--blk=", 6)==0)
		{
			sscanf(argv[arg]+6, "%lu", &blklen);
			last=true;
		}
		else if(strncmp(argv[arg], "--freq=", 7)==0)
		{
			sscanf(argv[arg]+7, "%lg", &centre);
		}
		else if(strncmp(argv[arg], "--slow=", 7)==0)
		{
			sscanf(argv[arg]+7, "%lg", &slow);
		}
		else if(strncmp(argv[arg], "--st=", 5)==0)
		{
			unsigned long st=32;
			sscanf(argv[arg]+5, "%lu", &st);
			slow=st/32.0;
		}
		else if(strncmp(argv[arg], "--if=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &aif);
		}
		else if(strncmp(argv[arg], "--am=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &am);
			last=false;
		}
		else if(strncmp(argv[arg], "--txb=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &txbaud);
		}
		else if(strcmp(argv[arg], "--moni")==0)
		{
			moni=true;
		}
		else if(strcmp(argv[arg], "--afc")==0)
		{
			afc=true;
		}
		else
		{
			fprintf(stderr, "Unrecognised arg `%s'\n", argv[arg]);
			return(1);
		}
	}
	double txcentre=centre;
	fprintf(stderr, "Constructing GUI\n");
	atg_canvas *canvas=atg_create_canvas(480, 240, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
	if(!canvas)
	{
		fprintf(stderr, "atg_create_canvas failed\n");
		return(1);
	}
	SDL_WM_SetCaption("3PSK", "3PSK");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	atg_box *mainbox=canvas->box;
	atg_element *g_title=atg_create_element_label("3psk: 0000Hz", 12, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
	if(!g_title)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_pack_element(mainbox, g_title))
	{
		perror("atg_pack_element");
		return(1);
	}
	if(!g_title->elem.label)
	{
		fprintf(stderr, "g_title->elem.label==NULL!\n");
		return(1);
	}
	char *g_title_label=g_title->elem.label->text;
	if(!g_title_label)
	{
		fprintf(stderr, "g_title_label==NULL!\n");
		return(1);
	}
	snprintf(g_title_label, 13, "3psk: %4dHz", (int)floor(centre+.5));
	atg_element *g_decoder=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
	if(!g_decoder)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_pack_element(mainbox, g_decoder))
	{
		perror("atg_pack_element");
		return(1);
	}
	char *g_bauds=NULL;
	bool *g_tx=NULL;
	SDL_Surface *g_constel_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 120, 120, canvas->surface->format->BitsPerPixel, canvas->surface->format->Rmask, canvas->surface->format->Gmask, canvas->surface->format->Bmask, canvas->surface->format->Amask);
	if(!g_constel_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	SDL_Surface *g_phasing_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 100, 120, canvas->surface->format->BitsPerPixel, canvas->surface->format->Rmask, canvas->surface->format->Gmask, canvas->surface->format->Bmask, canvas->surface->format->Amask);
	if(!g_phasing_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	else
	{
		SDL_FillRect(g_constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(g_constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
		SDL_FillRect(g_phasing_img, &(SDL_Rect){0, 0, 100, 120}, SDL_MapRGB(g_constel_img->format, PHAS_BG.r, PHAS_BG.g, PHAS_BG.b));
		atg_box *g_db=g_decoder->elem.box;
		if(!g_db)
		{
			fprintf(stderr, "g_decoder->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_constel=atg_create_element_image(g_constel_img);
		if(!g_constel)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_constel))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_phasing=atg_create_element_image(g_phasing_img);
		if(!g_phasing)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_phasing))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_controls=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE});
		if(!g_controls)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_controls))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_box *b=g_controls->elem.box;
		if(!b)
		{
			fprintf(stderr, "g_controls->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_btns1=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 23, 23, ATG_ALPHA_OPAQUE});
		if(!g_btns1)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(b, g_btns1))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_box *bb1=g_btns1->elem.box;
		if(!bb1)
		{
			fprintf(stderr, "g_btns1->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_tx_t=atg_create_element_toggle("TX", false, (atg_colour){191, 0, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_tx_t)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		atg_toggle *t=g_tx_t->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_tx_t->elem.toggle==NULL\n");
			return(1);
		}
		g_tx=&t->state;
		g_tx_t->userdata="TX";
		if(atg_pack_element(bb1, g_tx_t))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_moni=atg_create_element_toggle("MONI", moni, (atg_colour){0, 191, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_moni)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		g_moni->userdata="MONI";
		if(atg_pack_element(bb1, g_moni))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_afc=atg_create_element_toggle("AFC", moni, (atg_colour){191, 127, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_afc)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		g_afc->userdata="AFC";
		if(atg_pack_element(bb1, g_afc))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_txbaud=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 300, 1, txbaud, "TXB %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_txbaud)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_txbaud->userdata="TXBAUD";
		if(atg_pack_element(b, g_txbaud))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_slow=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 512, 1, floor(slow*32+.5), "RXS %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_slow)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_slow->userdata="SLOW";
		if(atg_pack_element(b, g_slow))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_amp=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 25, 1, floor(am*5+.5), "AMP %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_amp)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_amp->userdata="AMP";
		if(atg_pack_element(b, g_amp))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_baud_label=atg_create_element_label("RXB 000", 15, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
		if(!g_baud_label)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!g_baud_label->elem.label)
		{
			fprintf(stderr, "g_baud_label->elem.label==NULL\n");
			return(1);
		}
		g_bauds=g_baud_label->elem.label->text;
		if(!g_bauds)
		{
			fprintf(stderr, "g_bauds==NULL\n");
			return(1);
		}
		if(atg_pack_element(b, g_baud_label))
		{
			perror("atg_pack_element");
			return(1);
		}
		// TODO make spectrogram, other decoder bits?
	}
	char *outtext[OUTLINES];
	for(unsigned int i=0;i<OUTLINES;i++)
	{
		atg_element *line=atg_create_element_label(NULL, 8, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
		if(!line)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!line->elem.label)
		{
			fprintf(stderr, "line->elem.label==NULL\n");
			return(1);
		}
		outtext[i]=line->elem.label->text=malloc(OUTLINELEN+2);
		if(!outtext[i])
		{
			fprintf(stderr, "outtext[i]==NULL\n");
			return(1);
		}
		outtext[i][0]=' ';
		outtext[i][1]=0;
		if(atg_pack_element(mainbox, line))
		{
			perror("atg_pack_element");
			return(1);
		}
	}
	char intextleft[INLINELEN+2]=" ", intextright[INLINELEN+1]="";
	atg_element *in_line=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){7, 7, 31, ATG_ALPHA_OPAQUE});
	if(!in_line)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_pack_element(mainbox, in_line))
	{
		perror("atg_pack_element");
		return(1);
	}
	else
	{
		atg_box *b=in_line->elem.box;
		if(!b)
		{
			fprintf(stderr, "in_line->elem.box==NULL\n");
			return(1);
		}
		atg_element *left=atg_create_element_label(NULL, 8, (atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE});
		if(!left)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!left->elem.label)
		{
			fprintf(stderr, "left->elem.label==NULL\n");
			return(1);
		}
		left->elem.label->text=intextleft;
		if(atg_pack_element(b, left))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *right=atg_create_element_label(NULL, 8, (atg_colour){255, 127, 0, ATG_ALPHA_OPAQUE});
		if(!right)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!right->elem.label)
		{
			fprintf(stderr, "right->elem.label==NULL\n");
			return(1);
		}
		right->elem.label->text=intextright;
		if(atg_pack_element(b, right))
		{
			perror("atg_pack_element");
			return(1);
		}
	}
	
	fprintf(stderr, "Setting up decoder frontend\n");
	wavhdr w;
	if(read_wh44(stdin, &w))
	{
		fprintf(stderr, "Failed to read WAV header; werrno=%d\n", werrno);
		return(1);
	}
	if(w.num_channels!=1) // TODO handle a stereo I/Q (ie. complex) input
	{
		fprintf(stderr, "Too many channels; input WAV stream should be mono\n");
		return(1);
	}
	
	fprintf(stderr, "frontend: Audio sample rate: %lu Hz\n", (unsigned long)w.sample_rate);
	if(!last)
		blklen=floor(w.sample_rate/bw);
	bw=w.sample_rate/(double)blklen;
	
	fprintf(stderr, "frontend: FFT block length: %lu samples\n", blklen);
	if(blklen>4095)
	{
		fprintf(stderr, "frontend: FFT block length too long.  Exiting\n");
		return(1);
	}
	fprintf(stderr, "frontend: Filter bandwidth is %g Hz\n", bw);
	unsigned long k = max(floor((aif * (double)blklen / (double)w.sample_rate)+0.5), 1);
	double truif=(k*w.sample_rate/(double)blklen);
	fprintf(stderr, "frontend: Actual IF: %g Hz\n", truif);
	fftw_complex *fftin=fftw_malloc(sizeof(fftw_complex) * blklen);
	fftw_complex *fftout=fftw_malloc(sizeof(fftw_complex) * blklen);
	fftw_set_timelimit(2.0);
	fftw_plan p=fftw_plan_dft_1d(blklen, fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	long wzero=zeroval(w);
	
	fprintf(stderr, "Setting up decoder\n");
	double gsf=250.0/(double)blklen;
	double sens=(double)blklen/16.0;
	
	fftw_complex points[CONSLEN];
	bool lined[CONSLEN];
	for(size_t i=0;i<CONSLEN;i++)
	{
		points[i]=0;
		lined[i]=false;
	}
	fftw_complex lastsym=0;
	
	bool bitbuf[BITBUFLEN];
	unsigned int bitbufp=0;
	
	unsigned int da_ptr=0;
	double old_da[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		old_da[i]=0;
	double t_da=0;
	bool fch=false;
	unsigned int st_ptr=0;
	unsigned int symtime[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		symtime[i]=0;
	bool st_loop=false;
	
	bool transmit=false;
	double txphi=0;
	bbuf txbits={0, NULL};
	unsigned int txsetp=0;
	unsigned int txlead=0;
	
	if(write_wh44(stdout, w))
	{
		fprintf(stderr, "Failed to write WAV header\n");
		return(1);
	}
	int errupt=0;
	fprintf(stderr, "Starting main loop\n");
	for(unsigned int t=0;((t<w.num_samples)||(w.data_len==(uint32_t)(-1)))&&!errupt;t++)
	{
		if(!(t%blklen))
		{
			static unsigned int frame=0;
			fftw_execute(p);
			int x,y;
			ztoxy(points[frame%CONSLEN], gsf, &x, &y);
			if(lined[frame%CONSLEN]) line(g_constel_img, x, y, 60, 60, CONS_BG);
			pset(g_constel_img, x, y, CONS_BG);
			fftw_complex half=points[(frame+(CONSLEN>>1))%CONSLEN];
			ztoxy(half, gsf, &x, &y);
			atg_colour c=(cabs(half)>sens)?(atg_colour){0, 127, 0, ATG_ALPHA_OPAQUE}:(atg_colour){127, 0, 0, ATG_ALPHA_OPAQUE};
			if(lined[(frame+(CONSLEN>>1))%CONSLEN]) line(g_constel_img, x, y, 60, 60, (atg_colour){0, 95, 95, ATG_ALPHA_OPAQUE});
			pset(g_constel_img, x, y, c);
			ztoxy(points[frame%CONSLEN]=fftout[k], gsf, &x, &y);
			bool green=cabs(fftout[k])>sens;
			bool enough=false;
			double da=0;
			if(cabs(lastsym)>sens)
				enough=(fabs(da=carg(fftout[k]/lastsym))>(fch?M_PI*2/3.0:M_PI/2));
			else
				enough=true;
			fftw_complex dz=fftout[k]-points[(frame+CONSLEN-1)%CONSLEN];
			if((lined[frame%CONSLEN]=(green&&enough&&(fch||(cabs(dz)<cabs(fftout[k])*blklen/(slow*25.0))))))
			{
				line(g_constel_img, x, y, 60, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				line(g_phasing_img, 0, 60, g_phasing_img->w, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				int py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(g_phasing_img, da_ptr*g_phasing_img->w/PHASLEN, py, (da_ptr+1)*g_phasing_img->w/PHASLEN, py, PHAS_BG);
				old_da[da_ptr]=(da<0)?da+M_PI*4/3.0:da;
				t_da+=old_da[da_ptr]-M_PI*2/3.0;
				py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(g_phasing_img, da_ptr*g_phasing_img->w/PHASLEN, py, (da_ptr+1)*g_phasing_img->w/PHASLEN, py, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
				unsigned int dt=t-symtime[st_ptr];
				double baud=w.sample_rate*(st_loop?PHASLEN:st_ptr)/(double)dt;
				snprintf(g_bauds, 8, "RXB %03d", (int)floor(baud+.5));
				symtime[st_ptr]=t;
				st_ptr=(st_ptr+1)%PHASLEN;
				if(!st_ptr) st_loop=true;
				da_ptr=(da_ptr+1)%PHASLEN;
				if(afc&&!da_ptr)
				{
					double ch=(t_da/(double)PHASLEN)*5.0;
					if(fabs(ch)>1.0)
					{
						centre+=ch;
						snprintf(g_title_label, 13, "3psk: %4dHz", (int)floor(centre+.5));
						fch=true;
					}
					t_da=0;
				}
				lastsym=fftout[k];
				if(bitbufp>=BITBUFLEN)
				{
					bitbufp--;
					for(unsigned int i=0;i<bitbufp;i++)
						bitbuf[i]=bitbuf[i+1];
				}
				bitbuf[bitbufp++]=(da>0);
				int ubits=0;
				char *text=decode((bbuf){.nbits=bitbufp, .data=bitbuf}, &ubits);
				if(*text)
				{
					size_t tp=strlen(outtext[OUTLINES-1]);
					for(const char *p=text;*p;p++)
					{
						if((*p=='\n')||(tp>OUTLINELEN))
						{
							for(unsigned int i=0;i<OUTLINES-1;i++)
								strcpy(outtext[i], outtext[i+1]);
							outtext[OUTLINES-1][0]=' ';
							outtext[OUTLINES-1][1]=0;
							tp=1;
						}
						if(*p=='\t')
						{
							outtext[OUTLINES-1][tp++]=' ';
							while((tp<OUTLINELEN)&&((tp-1)&3))
								outtext[OUTLINES-1][tp++]=' ';
							outtext[OUTLINES-1][tp]=0;
						}
						else if(*p!='\n')
						{
							outtext[OUTLINES-1][tp++]=*p;
							outtext[OUTLINES-1][tp]=0;
						}
					}
				}
				bitbufp-=ubits;
				for(unsigned int i=0;i<bitbufp;i++)
					bitbuf[i]=bitbuf[i+ubits];
				free(text);
			}
			fch=false;
			pset(g_constel_img, x, y, green?(atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE}:(atg_colour){255, 0, 0, ATG_ALPHA_OPAQUE});
			if(!(frame++%2))
			{
				if(g_tx) *g_tx=transmit;
				atg_flip(canvas);
				atg_event e;
				while(atg_poll_event(&e, canvas))
				{
					switch(e.type)
					{
						case ATG_EV_RAW:;
							SDL_Event s=e.event.raw;
							switch(s.type)
							{
								case SDL_QUIT:
									errupt=1;
								break;
								case SDL_KEYDOWN:
									switch(s.key.keysym.sym)
									{
										case SDLK_F7:
											transmit=true;
											txlead=8;
										break;
										case SDLK_F8:
											transmit=false;
										break;
										case SDLK_ESCAPE:
											transmit=false;
											intextright[0]=0;
										break;
										case SDLK_F9:
											centre=txcentre;
											snprintf(g_title_label, 13, "3psk: %4dHz", (int)floor(centre+.5));
										break;
										case SDLK_BACKSPACE:
										{
											size_t tp=strlen(intextright);
											if(tp) intextright[tp-1]=0;
										}
										break;
										case SDLK_RETURN:
											addchar(intextleft, intextright, '\n');
										break;
										case SDLK_KP_ENTER:
											addchar(intextleft, intextright, '\r');
										break;
										default:
											if((s.key.keysym.unicode&0xFF80)==0)
											{
												char what=s.key.keysym.unicode&0x7F;
												addchar(intextleft, intextright, what);
											}
										break;
									}
								break;
							}
						break;
						case ATG_EV_TRIGGER:;
							atg_ev_trigger trigger=e.event.trigger;
							if(!trigger.e)
							{
								// internal error
							}
							else
								fprintf(stderr, "Clicked on unknown button!\n");
						break;
						case ATG_EV_VALUE:;
							atg_ev_value value=e.event.value;
							if(value.e)
							{
								if(value.e->userdata)
								{
									if(strcmp((const char *)value.e->userdata, "SLOW")==0)
									{
										slow=value.value/32.0;
									}
									else if(strcmp((const char *)value.e->userdata, "TXBAUD")==0)
									{
										txbaud=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "AMP")==0)
									{
										am=value.value/5.0;
									}
									else
										fprintf(stderr, "Changed an unknown spinner!\n");
								}
							}
						break;
						case ATG_EV_TOGGLE:;
							atg_ev_toggle toggle=e.event.toggle;
							if(toggle.e)
							{
								if(toggle.e->userdata)
								{
									if(strcmp((const char *)toggle.e->userdata, "TX")==0)
									{
										transmit=toggle.state;
									}
									else if(strcmp((const char *)toggle.e->userdata, "MONI")==0)
									{
										moni=toggle.state;
									}
									else if(strcmp((const char *)toggle.e->userdata, "AFC")==0)
									{
										if(!(afc=toggle.state))
										{
											centre=txcentre;
											snprintf(g_title_label, 13, "3psk: %4dHz", (int)floor(centre+.5));
										}
									}
									else
										fprintf(stderr, "Clicked on unknown toggle '%s'!\n", (const char *)toggle.e->userdata);
								}
							}
						break;
						default:
						break;
					}
				}
			}
		}
		long si=read_sample(w, stdin)-wzero;
		if(transmit)
		{
			if(((t*txbaud)%w.sample_rate)<txbaud)
			{
				if(txlead)
				{
					txlead--;
					txsetp=(txsetp+2)%3;
				}
				else
				{
					if(!txbits.data) txbits.nbits=0;
					if(*intextright&&!txbits.nbits)
					{
						free(txbits.data);
						const char buf[2]={*intextright, 0};
						size_t ll=strlen(intextleft);
						if(ll>INLINELEN)
						{
							size_t i;
							for(i=1;i+20<ll;i++)
								intextleft[i]=intextleft[i+20];
							intextleft[i]=0;
							ll=i;
						}
						intextleft[ll++]=*intextright;
						intextleft[ll]=0;
						if(*intextright=='\r')
						{
							transmit=false;
							txbits=(bbuf){0, NULL};
						}
						else
							txbits=encode(buf);
						for(size_t i=0;intextright[i];i++)
							intextright[i]=intextright[i+1];
					}
					if(txbits.nbits)
					{
						bool b=*txbits.data;
						txbits.nbits--;
						for(size_t i=0;i<txbits.nbits;i++)
							txbits.data[i]=txbits.data[i+1];
						txsetp=(txsetp+(b?1:2))%3;
					}
					else
					{
						txsetp=(txsetp+2)%3;
					}
				}
			}
			double sweep=txbaud*M_PI*1.5/(double)w.sample_rate;
			double txaim=txsetp*M_PI*2/3.0;
			if((txphi>txaim-M_PI)&&(txphi<=txaim))
				txphi=min(txphi+sweep, txaim);
			else if(txphi>txaim+M_PI)
				txphi=fmod(txphi+sweep, M_PI*2);
			else if(txphi<=txaim-M_PI)
				txphi=fmod(txphi+M_PI*2-sweep, M_PI*2);
			else if((txphi>txaim)&&(txphi<=txaim+M_PI))
				txphi=max(txphi-sweep, txaim);
			else
			{
				fprintf(stderr, "Impossible txphi/aim relationship!\n");
				txphi=txaim;
			}
			double txmag=cos(M_PI/3)/cos(fmod(txphi, M_PI*2/3.0)-M_PI/3.0);
			double ft=t*2*M_PI*txcentre/w.sample_rate;
			double tx=cos(ft+txphi)*txmag/3.0;
			if(wzero) tx+=0.5;
			write_sample(w, stdout, tx*(1<<w.bits_per_sample));
			if(moni) si=tx*(1<<w.bits_per_sample)*.6-wzero;
		}
		else
			write_sample(w, stdout, wzero);
		if(wzero) si<<=1;
		double sv=si/(double)(1<<w.bits_per_sample);
		double phi=t*2*M_PI*(truif-centre)/w.sample_rate;
		fftin[t%blklen]=sv*(cos(phi)+I*sin(phi))*am;
	}
	fprintf(stderr, "\n");
	return(0);
}

int pset(SDL_Surface *s, unsigned int x, unsigned int y, atg_colour c)
{
	if(!s)
		return(1);
	if((x>=(unsigned int)s->w)||(y>=(unsigned int)s->h))
		return(2);
	size_t s_off = (y*s->pitch) + (x*s->format->BytesPerPixel);
	uint32_t pixval = SDL_MapRGBA(s->format, c.r, c.g, c.b, c.a);
	*(uint32_t *)((char *)s->pixels + s_off)=pixval;
	return(0);
}

int line(SDL_Surface *s, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, atg_colour c)
{
	// Bresenham's line algorithm, based on http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	int e=0;
	if((e=pset(s, x0, y0, c)))
		return(e);
	bool steep = abs(y1 - y0) > abs(x1 - x0);
	int tmp;
	if(steep)
	{
		tmp=x0;x0=y0;y0=tmp;
		tmp=x1;x1=y1;y1=tmp;
	}
	if(x0>x1)
	{
		tmp=x0;x0=x1;x1=tmp;
		tmp=y0;y0=y1;y1=tmp;
	}
	int dx=x1-x0,dy=abs(y1-y0);
	int ey=dx>>1;
	int dely=(y0<y1?1:-1),y=y0;
	for(int x=x0;x<(int)x1;x++)
	{
		if((e=pset(s, steep?y:x, steep?x:y, c)))
			return(e);
		ey-=dy;
		if(ey<0)
		{
			y+=dely;
			ey+=dx;
		}
	}
	return(0);
}

void ztoxy(fftw_complex z, double gsf, int *x, int *y)
{
	if(x) *x=creal(z)*gsf+60;
	if(y) *y=cimag(z)*gsf+60;
}

void addchar(char *intextleft, char *intextright, char what)
{
	size_t tp=strlen(intextright);
	if(tp<INLINELEN)
	{
		intextright[tp++]=what;
		intextright[tp]=0;
		size_t ll=strlen(intextleft);
		if(tp+ll>=INLINELEN)
		{
			size_t i;
			for(i=1;i+20<ll;i++)
				intextleft[i]=intextleft[i+20];
			intextleft[i]=0;
		}
	}
	else
	{
		fprintf(stderr, "Input overrun - discarded char %hhd\n", what);
	}
}

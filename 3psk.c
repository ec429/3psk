/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include <atg.h>
#include "wav.h"
#include "bits.h"
#include "varicode.h"
#include "strbuf.h"
#include "gui.h"

#define CONSLEN	1024
#define CONSDLEN	((int)floor(CONSLEN*min(bw, 750)/750))
#define BITBUFLEN	16
#define PHASLEN	25

void ztoxy(fftw_complex z, double gsf, int *x, int *y);

int main(int argc, char **argv)
{
	double txf=440; // centre frequency, Hz
	double rxf=txf;
	bool setrxf=false;
	double aif=3000; // approximate IF, Hz
	unsigned int rxs=28;
	unsigned int amp=10;
	bool init_moni=true, init_afc=false;
	unsigned int txb=60;
	unsigned int bws=2; // TODO: --bws= option, and --bw= (will need a list)
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--txf=", 6)==0)
		{
			sscanf(argv[arg]+7, "%lg", &txf);
		}
		else if(strncmp(argv[arg], "--rxf=", 6)==0)
		{
			sscanf(argv[arg]+7, "%lg", &rxf);
			setrxf=true;
		}
		else if(strncmp(argv[arg], "--rxs=", 6)==0)
		{
			sscanf(argv[arg]+7, "%u", &rxs);
		}
		else if(strncmp(argv[arg], "--if=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &aif);
		}
		else if(strncmp(argv[arg], "--amp=", 6)==0)
		{
			sscanf(argv[arg]+5, "%u", &amp);
		}
		else if(strncmp(argv[arg], "--txb=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &txb);
		}
		else if(strcmp(argv[arg], "--moni")==0)
		{
			init_moni=true;
		}
		else if(strcmp(argv[arg], "--no-moni")==0)
		{
			init_moni=false;
		}
		else if(strcmp(argv[arg], "--afc")==0)
		{
			init_afc=true;
		}
		else if(strcmp(argv[arg], "--no-afc")==0)
		{
			init_afc=false;
		}
		else
		{
			fprintf(stderr, "Unrecognised arg `%s'\n", argv[arg]);
			return(1);
		}
	}
	if(!setrxf) rxf=txf;
	fprintf(stderr, "Constructing GUI\n");
	gui G;
	{
		int e;
		if((e=make_gui(&G, &bws)))
		{
			fprintf(stderr, "Failed to construct GUI\n");
			return(e);
		}
		if(G.g_tx) *G.g_tx=false; else {fprintf(stderr, "Error: G.g_tx is NULL\n"); return(1);}
		if(G.g_moni) *G.g_tx=init_moni; else {fprintf(stderr, "Error: G.g_moni is NULL\n"); return(1);}
		if(G.g_afc) *G.g_tx=init_afc; else {fprintf(stderr, "Error: G.g_afc is NULL\n"); return(1);}
		if(G.g_spl) *G.g_tx=!setrxf; else {fprintf(stderr, "Error: G.g_spl is NULL\n"); return(1);}
	}
	
	unsigned int inp=NMACROS;
	
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
	unsigned long blklen=floor(w.sample_rate/150.0);
	double bw=w.sample_rate/(double)blklen;
	
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
	fftw_complex *fftin=fftw_malloc(sizeof(fftw_complex)*floor(w.sample_rate/10.0));
	fftw_complex *fftout=fftw_malloc(sizeof(fftw_complex)*floor(w.sample_rate/10.0));
	unsigned long speclen=max(floor(w.sample_rate/5.0), 360), spechalf=(speclen>>1)+1;
	double spec_hpp=w.sample_rate/(double)speclen;
	double *specin=fftw_malloc(sizeof(double)*speclen);
	fftw_complex *specout=fftw_malloc(sizeof(fftw_complex)*spechalf);
	fftw_set_timelimit(2.0);
	fprintf(stderr, "frontend: Preparing FFT plans\n");
	fftw_plan p[4], sp_p;
	p[0]=fftw_plan_dft_1d(floor(w.sample_rate/10.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[1]=fftw_plan_dft_1d(floor(w.sample_rate/30.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[2]=fftw_plan_dft_1d(floor(w.sample_rate/150.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[3]=fftw_plan_dft_1d(floor(w.sample_rate/750.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	sp_p=fftw_plan_dft_r2c_1d(speclen, specin, specout, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	fprintf(stderr, "frontend: Ready\n");
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
	
	unsigned int spec_pt[8][160];
	for(unsigned int i=0;i<8;i++)
		for(unsigned int j=0;j<160;j++)
			spec_pt[i][j]=60;
	unsigned int spec_which=0;
	
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
			static unsigned int frame=0, lastflip=0;
			fftw_execute(p[bws]);
			int x,y;
			ztoxy(points[frame%CONSDLEN], gsf, &x, &y);
			if(lined[frame%CONSDLEN]) line(G.g_constel_img, x, y, 60, 60, CONS_BG);
			pset(G.g_constel_img, x, y, CONS_BG);
			fftw_complex half=points[(frame+(CONSDLEN>>1))%CONSDLEN];
			ztoxy(half, gsf, &x, &y);
			atg_colour c=(cabs(half)>sens)?(atg_colour){0, 127, 0, ATG_ALPHA_OPAQUE}:(atg_colour){127, 0, 0, ATG_ALPHA_OPAQUE};
			if(lined[(frame+(CONSDLEN>>1))%CONSDLEN]) line(G.g_constel_img, x, y, 60, 60, (atg_colour){0, 95, 95, ATG_ALPHA_OPAQUE});
			pset(G.g_constel_img, x, y, c);
			ztoxy(points[frame%CONSDLEN]=fftout[k], gsf, &x, &y);
			bool green=cabs(fftout[k])>sens;
			bool enough=false;
			double da=0;
			if(cabs(lastsym)>sens)
				enough=(fabs(da=carg(fftout[k]/lastsym))>(fch?M_PI*2/3.0:M_PI/2));
			else
				enough=true;
			fftw_complex dz=bws?fftout[k]-points[(frame+CONSDLEN-1)%CONSDLEN]:fftout[k]/points[(frame+CONSDLEN-1)%CONSDLEN];
			bool spd=bws?(cabs(dz)<cabs(fftout[k])*blklen*blklen/(exp2(((signed)rxs-32)/4.0)*2e4)):(fabs(carg(dz))<blklen/(exp2(((signed)rxs-32)/4.0)*2e2));
			if((lined[frame%CONSDLEN]=(green&&enough&&(fch||spd))))
			{
				line(G.g_constel_img, x, y, 60, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				line(G.g_phasing_img, 0, 60, G.g_phasing_img->w, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				int py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(G.g_phasing_img, da_ptr*G.g_phasing_img->w/PHASLEN, py, (da_ptr+1)*G.g_phasing_img->w/PHASLEN, py, PHAS_BG);
				old_da[da_ptr]=(da<0)?da+M_PI*4/3.0:da;
				t_da+=old_da[da_ptr]-M_PI*2/3.0;
				py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(G.g_phasing_img, da_ptr*G.g_phasing_img->w/PHASLEN, py, (da_ptr+1)*G.g_phasing_img->w/PHASLEN, py, (da>0)?(atg_colour){255, 191, 255, ATG_ALPHA_OPAQUE}:(atg_colour){255, 255, 127, ATG_ALPHA_OPAQUE});
				unsigned int dt=t-symtime[st_ptr];
				double baud=w.sample_rate*(st_loop?PHASLEN:st_ptr)/(double)dt;
				snprintf(G.g_bauds, 8, "RXB %03d", (int)floor(baud+.5));
				symtime[st_ptr]=t;
				st_ptr=(st_ptr+1)%PHASLEN;
				if(!st_ptr) st_loop=true;
				da_ptr=(da_ptr+1)%PHASLEN;
				if(G.g_afc&&*G.g_afc&&!da_ptr)
				{
					double ch=(t_da/(double)PHASLEN)*10.0;
					if(fabs(ch)>0.5)
					{
						rxf+=ch;
						if(G.g_rxf&&(G.g_rxf->type==ATG_SPINNER))
						{
							atg_spinner *s=G.g_rxf->elem.spinner;
							if(s) s->value=floor(rxf+.5);
						}
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
					size_t tp=strlen(G.outtext[OUTLINES-1]);
					for(const char *p=text;*p;p++)
					{
						if((*p=='\n')||(tp>OUTLINELEN))
						{
							for(unsigned int i=0;i<OUTLINES-1;i++)
								strcpy(G.outtext[i], G.outtext[i+1]);
							G.outtext[OUTLINES-1][0]=' ';
							G.outtext[OUTLINES-1][1]=0;
							tp=1;
						}
						if(*p=='\t')
						{
							G.outtext[OUTLINES-1][tp++]=' ';
							while((tp<OUTLINELEN)&&((tp-1)&3))
								G.outtext[OUTLINES-1][tp++]=' ';
							G.outtext[OUTLINES-1][tp]=0;
						}
						else if(*p!='\n')
						{
							G.outtext[OUTLINES-1][tp++]=*p;
							G.outtext[OUTLINES-1][tp]=0;
						}
					}
				}
				bitbufp-=ubits;
				for(unsigned int i=0;i<bitbufp;i++)
					bitbuf[i]=bitbuf[i+ubits];
				free(text);
			}
			fch=false;
			pset(G.g_constel_img, x, y, green?(atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE}:(atg_colour){255, 0, 0, ATG_ALPHA_OPAQUE});
			frame++;
			if(t>(lastflip+w.sample_rate/8))
			{
				lastflip+=w.sample_rate/8;
				if(G.g_tx) *G.g_tx=transmit;
				if(G.g_spl) *G.g_spl=(rxf!=txf);
				if(true)
				{
					if(G.ingi>((INLINES+1)*INLINELEN))
					{
						G.ingi-=INLINELEN;
						memmove(G.ing, G.ing+INLINELEN, G.ingi);
					}
					for(unsigned int i=0;i<INLINES;i++)
						G.intextleft[i][0]=G.intextright[i][0]=0;
					unsigned int x=0,y=0;
					for(size_t p=0;p<G.ingi;p++)
					{
						G.intextleft[y][x++]=G.ing[p];
						G.intextleft[y][x]=0;
						if((G.ing[p]=='\n')||(x>=INLINELEN))
						{
							if(y<INLINES-1)
								y++;
							else
							{
								for(unsigned int i=0;i<y;i++)
									strcpy(G.intextleft[i], G.intextleft[i+1]);
								G.intextleft[y][0]=0;
							}
							x=0;
						}
					}
					unsigned int sx=0;
					for(size_t p=0;p<G.inri;p++)
					{
						x++;
						G.intextright[y][sx++]=G.inr[p];
						G.intextright[y][sx]=0;
						if((G.inr[p]=='\n')||(x>=INLINELEN))
						{
							if(y<INLINES-1)
								y++;
							else
							{
								for(unsigned int i=0;i<y;i++)
								{
									strcpy(G.intextleft[i], G.intextleft[i+1]);
									strcpy(G.intextright[i], G.intextright[i+1]);
								}
								G.intextleft[y][0]=G.intextright[y][0]=0;
							}
							x=sx=0;
						}
					}
				}
				for(unsigned int i=0;i<NMACROS;i++)
					*G.mcol[i]=(i==inp)?(atg_colour){63, 63, 47, ATG_ALPHA_OPAQUE}:(atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE};
				atg_flip(G.canvas);
				atg_event e;
				while(atg_poll_event(&e, G.canvas))
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
										case SDLK_F1:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[0]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F2:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[1]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F3:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[2]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F4:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[3]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F5:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[4]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F6:
											append_str(&G.inr, &G.inrl, &G.inri, G.macro[5]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txb, 8);
											}
										break;
										case SDLK_F7:
											if(G.g_tx) *G.g_tx=true;
											txlead=max(txb, 8);
										break;
										case SDLK_F8:
											if(G.g_tx) *G.g_tx=false;
											txlead=max(txb/2, 8);
										break;
										case SDLK_ESCAPE:
											if(G.g_tx) *G.g_tx=false;
											txlead=0;
											G.inri=0;
										break;
										case SDLK_F9:
											rxf=txf;
											if(G.g_rxf&&(G.g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=G.g_rxf->elem.spinner;
												if(s) s->value=floor(rxf+.5);
											}
										break;
										case SDLK_BACKSPACE:
										{
											if(inp>=NMACROS)
											{
												if(G.inri) G.inr[--G.inri]=0;
											}
											else
											{
												size_t l=strlen(G.macro[inp]);
												if(l) G.macro[inp][l-1]=0;
											}
										}
										break;
										case SDLK_RETURN:
											if(inp>=NMACROS)
												append_char(&G.inr, &G.inrl, &G.inri, '\n');
											else
											{
												size_t l=strlen(G.macro[inp]);
												if(l<MACROLEN-1)
												{
													G.macro[inp][l++]='\n';
													G.macro[inp][l]=0;
												}
											}
										break;
										case SDLK_KP_ENTER:
											if(inp>=NMACROS)
												append_char(&G.inr, &G.inrl, &G.inri, '\r');
											else
											{
												size_t l=strlen(G.macro[inp]);
												if(l<MACROLEN-1)
												{
													G.macro[inp][l++]='\r';
													G.macro[inp][l]=0;
												}
											}
										break;
										default:
											if((s.key.keysym.unicode&0xFF80)==0)
											{
												char what=s.key.keysym.unicode&0x7F;
												if(what)
												{
													if(inp>=NMACROS)
														append_char(&G.inr, &G.inrl, &G.inri, what);
													else
													{
														size_t l=strlen(G.macro[inp]);
														if(l<MACROLEN-1)
														{
															G.macro[inp][l++]=what;
															G.macro[inp][l]=0;
														}
													}
												}
											}
										break;
									}
								break;
							}
						break;
						case ATG_EV_CLICK:;
							atg_ev_click click=e.event.click;
							if(!click.e)
								fprintf(stderr, "click.e==NULL\n");
							else if(click.e->userdata)
							{
								if(strcmp(click.e->userdata, "SPEC")==0)
								{
									switch(click.button)
									{
										case ATG_MB_LEFT:
											txf=min(max((click.pos.x+20)*spec_hpp, 200), 800);
											if(G.g_txf&&(G.g_txf->type==ATG_SPINNER))
											{
												atg_spinner *s=G.g_txf->elem.spinner;
												if(s) s->value=floor(txf+.5);
											}
											/* fallthrough */
										case ATG_MB_RIGHT:
											rxf=min(max((click.pos.x+20)*spec_hpp, 200), 800);
											if(G.g_rxf&&(G.g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=G.g_rxf->elem.spinner;
												if(s) s->value=floor(rxf+.5);
											}
										break;
										default:
											// ignore
										break;
									}
								}
							}
							else
							{
								for(unsigned int i=0;i<NMACROS;i++)
									if(click.e==G.mline[i]) inp=(inp==i)?NMACROS:i;
							}
						break;
						case ATG_EV_TRIGGER:;
							atg_ev_trigger trigger=e.event.trigger;
							if(!trigger.e)
							{
								fprintf(stderr, "trigger.e==NULL\n");
							}
							else
								fprintf(stderr, "Clicked on unknown button!\n");
						break;
						case ATG_EV_VALUE:;
							atg_ev_value value=e.event.value;
							if(value.e)
							{
								if(value.e==G.g_bw)
								{
									bw=(double[4]){10, 30, 150, 750}[value.value];
									blklen=floor(w.sample_rate/bw);
									bw=w.sample_rate/(double)blklen;
									fprintf(stderr, "frontend: new FFT block length: %lu samples\n", blklen);
									if(blklen>4095)
									{
										fprintf(stderr, "frontend: new FFT block length too long.  Exiting\n");
										return(1);
									}
									fprintf(stderr, "frontend: new filter bandwidth is %g Hz\n", bw);
									k=max(floor((aif * (double)blklen / (double)w.sample_rate)+0.5), 1);
									truif=(k*w.sample_rate/(double)blklen);
									fprintf(stderr, "frontend: new Actual IF: %g Hz\n", truif);
									gsf=250.0/(double)blklen;
									sens=(double)blklen/16.0;
									SDL_FillRect(G.g_constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(G.g_constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
									for(size_t i=0;i<CONSLEN;i++)
									{
										points[i]=0;
										lined[i]=false;
									}
									fprintf(stderr, "frontend: all ready to go with new bandwidth\n");
								}
								else if(value.e->userdata)
								{
									if(strcmp((const char *)value.e->userdata, "RXS")==0)
									{
										rxs=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "TXB")==0)
									{
										txb=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "TXF")==0)
									{
										bool spl=(rxf!=txf);
										txf=value.value;
										if(!spl)
										{
											rxf=txf;
											if(G.g_rxf&&(G.g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=G.g_rxf->elem.spinner;
												if(s) s->value=floor(rxf+.5);
											}
										}
									}
									else if(strcmp((const char *)value.e->userdata, "RXF")==0)
									{
										rxf=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "AMP")==0)
									{
										amp=value.value;
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
										txlead=max(txb/(transmit?1:2), 8);
									else if(strcmp((const char *)toggle.e->userdata, "MONI")==0)
									{
									}
									else if(strcmp((const char *)toggle.e->userdata, "AFC")==0)
									{
									}
									else if(strcmp((const char *)toggle.e->userdata, "SPL")==0)
									{
										if(!toggle.state)
										{
											rxf=txf;
											if(G.g_rxf&&(G.g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=G.g_rxf->elem.spinner;
												if(s) s->value=floor(rxf+.5);
											}
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
		if((G.g_tx&&*G.g_tx)||txlead)
		{
			if(((t*txb)%w.sample_rate)<txb)
			{
				if(txlead)
				{
					txlead--;
					txsetp=(txsetp+2)%3;
				}
				else
				{
					if(!txbits.data) txbits.nbits=0;
					if(G.inr&&G.inri&&!txbits.nbits)
					{
						free(txbits.data);
						const char buf[2]={G.inr[0], 0};
						if(*buf=='\r')
						{
							transmit=false;
							txlead=max(txb/2, 8);
							txbits=(bbuf){0, NULL};
						}
						else
							txbits=encode(buf);
						append_char(&G.ing, &G.ingl, &G.ingi, *buf);
						G.inri--;
						for(size_t i=0;i<G.inri;i++)
							G.inr[i]=G.inr[i+1];
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
			double sweep=txb*M_PI*1.5/(double)w.sample_rate;
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
			double ft=t*2*M_PI*txf/w.sample_rate;
			double tx=cos(ft+txphi)*txmag/3.0;
			if(wzero) tx+=0.5;
			write_sample(w, stdout, tx*(1<<w.bits_per_sample)*.8);
			if(G.g_moni&&*G.g_moni) si=tx*(1<<w.bits_per_sample)*.6-wzero;
		}
		else
			write_sample(w, stdout, wzero);
		if(wzero) si<<=1;
		double sv=si/(double)(1<<w.bits_per_sample);
		double phi=t*2*M_PI*(truif-rxf)/w.sample_rate;
		fftin[t%blklen]=sv*(cos(phi)+I*sin(phi))*amp/5.0;
		if(!(t%speclen))
		{
			fftw_execute(sp_p);
			SDL_FillRect(G.g_spectro_img, &(SDL_Rect){0, 0, 160, 60}, SDL_MapRGB(G.g_spectro_img->format, SPEC_BG.r, SPEC_BG.g, SPEC_BG.b));
			for(unsigned int h=1;h<9;h++)
			{
				unsigned int x=floor(h*100/spec_hpp)-20;
				line(G.g_spectro_img, x, 0, x, 59, (atg_colour){31, 31, 31, ATG_ALPHA_OPAQUE});
			}
			unsigned int rx=floor(rxf/spec_hpp)-20;
			line(G.g_spectro_img, rx, 0, rx, 59, (atg_colour){0, 47, 0, ATG_ALPHA_OPAQUE});
			unsigned int tx=floor(txf/spec_hpp)-20;
			line(G.g_spectro_img, tx, 0, tx, 59, (atg_colour){63, 0, 0, ATG_ALPHA_OPAQUE});
			for(unsigned int j=0;j<160;j++)
			{
				for(unsigned int k=1;k<8;k++)
					pset(G.g_spectro_img, j, spec_pt[(spec_which+k)%8][j], (atg_colour){k*20, k*20, 0, ATG_ALPHA_OPAQUE});
				double mag=4*sqrt(cabs(specout[j+20]));
				pset(G.g_spectro_img, j, spec_pt[spec_which][j]=max(59-mag, 0), (atg_colour){255, 255, 0, ATG_ALPHA_OPAQUE});
			}
			spec_which=(spec_which+1)%8;
		}
		specin[t%speclen]=sv;
	}
	fprintf(stderr, "\n");
	return(0);
}

void ztoxy(fftw_complex z, double gsf, int *x, int *y)
{
	if(x) *x=creal(z)*gsf+60;
	if(y) *y=cimag(z)*gsf+60;
}

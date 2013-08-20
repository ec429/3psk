/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include <atg.h>
#include "bits.h"
#include "varicode.h"
#include "strbuf.h"
#include "gui.h"
#include "frontend.h"
#include "audio.h"

#define CONSLEN	256
#define CONSDLEN	((int)floor(CONSLEN*min(bw, 750)/750))
#define BITBUFLEN	16
#define PHASLEN	25

#define txstart(lead)	if(G.tx&&!*G.tx) { *G.tx=true; txlead=max(txb(G), (lead)); }
#define txb(G)	(((G).txb&&(G).txb->elem.spinner)?(G).txb->elem.spinner->value:0)
#define txf(G)	(((G).txf&&(G).txf->elem.spinner)?(G).txf->elem.spinner->value:0)
#define rxs(G)	(((G).rxs&&(G).rxs->elem.spinner)?(G).rxs->elem.spinner->value:0)
#define amp(G)	(((G).amp&&(G).amp->elem.spinner)?(G).amp->elem.spinner->value:0)

void ztoxy(fftw_complex z, double gsf, int *x, int *y);

int main(int argc, char **argv)
{
	unsigned int init_txf=440; // centre frequency, Hz
	double rxf=init_txf;
	bool setrxf=false;
	double aif=3000; // approximate IF, Hz
	unsigned int init_rxs=28;
	unsigned int init_amp=10;
	bool init_moni=true, init_afc=false;
	unsigned int init_txb=60;
	unsigned int bws=2;
	unsigned int rxbuflen=AUDIOBUFLEN, txbuflen=AUDIOBUFLEN, sdlbuflen=AUDIOBUFLEN;
	char init_macro[6][MACROLEN];
	for(unsigned int i=0;i<6;i++)
		init_macro[i][0]=0;
	const char *wavrx=NULL;
	const char *conffile=NULL;
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--conf=", 7)==0)
			conffile=argv[arg]+7;
	}
	FILE *conffp=NULL;
	#ifdef WINDOWS
	if(!conffile)
		conffile="3psk.conf";
	#else
	if(!conffile)
	{
		const char *home=getenv("HOME");
		if(!home)
		{
			perror("No $HOME; getenv");
			return(1);
		}
		size_t n=strlen(home)+7;
		char *fn=malloc(n);
		if(!fn)
		{
			perror("malloc");
			return(1);
		}
		snprintf(fn, n, "%s/.3psk", home);
		conffp=fopen(fn, "r");
		if(!conffp)
		{
			fprintf(stderr, "Failed to open %s: fopen: %s\n", fn, strerror(errno));
			return(1);
		}
		free(fn);
	}
	else
	#endif
	{
		conffp=fopen(conffile, "r");
		if(!conffp)
		{
			fprintf(stderr, "Failed to open %s: fopen: %s\n", conffile, strerror(errno));
		}
	}
	if(conffp)
	{
		while(!feof(conffp))
		{
			char *line=fgetl(conffp);
			if(!line) break;
			switch(*line)
			{
				case '#': // comment
				case 0:
				break;
				default:
				{
					char *colon=strchr(line, ':');
					if(colon) *colon++=0;
					if((line[0]=='F')&&isdigit(line[1])&&!line[2])
					{
						int i=line[1]-'0'; // XXX possibly non-portable
						if((i<1)||(i>NMACROS))
						{
							fprintf(stderr, "Bad item in conffile: `F%d' (only F1 to F%d are allowed)\n", i, NMACROS);
							return(1);
						}
						size_t j=0;
						while((j<MACROLEN)&&colon&&*colon)
						{
							char p=*colon++;
							switch(p)
							{
								case '\\':
									p=*colon++;
									switch(p)
									{
										case 'n':
											p='\n';
										break;
										case 't':
											p='\t';
										break;
										case 'r':
											p='\r';
										break;
									}
								break;
								default:
								break;
							}
							init_macro[i-1][j++]=p;
							init_macro[i-1][j]=0;
						}
					}
					else if(strcmp(line, "TXF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &init_txf)!=1)
							{
								fprintf(stderr, "Bad TXF in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad TXF in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "RXF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%lg", &rxf)!=1)
							{
								fprintf(stderr, "Bad RXF in conffile: %s not numeric\n", colon);
								return(1);
							}
							setrxf=true;
						}
						else
						{
							fprintf(stderr, "Bad RXF in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "TXB")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &init_txb)!=1)
							{
								fprintf(stderr, "Bad TXB in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad TXB in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "RXS")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &init_rxs)!=1)
							{
								fprintf(stderr, "Bad RXS in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad RXS in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "AMP")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &init_amp)!=1)
							{
								fprintf(stderr, "Bad AMP in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad AMP in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "BW")==0)
					{
						if(colon)
						{
							unsigned int bw;
							if(sscanf(colon, "%u", &bw)!=1)
							{
								fprintf(stderr, "Bad BW in conffile: %s not numeric\n", colon);
								return(1);
							}
							unsigned int i;
							for(i=0;i<4;i++)
								if(bw==bandwidths[i])
									break;
							if(i<4)
								bws=i;
							else
							{
								fprintf(stderr, "Bad BW in conffile: invalid value %u\n", bw);
								fprintf(stderr, "\tValid values are: %u", bandwidths[0]);
								for(unsigned int i=1;i<4;i++)
									fprintf(stderr, ", %u", bandwidths[i]);
								fprintf(stderr, "\n");
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad BW in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "IF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%lg", &aif)!=1)
							{
								fprintf(stderr, "Bad IF in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad IF in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "MONI")==0)
						init_moni=true;
					else if(strcmp(line, "!MONI")==0)
						init_moni=false;
					else if(strcmp(line, "AFC")==0)
						init_afc=true;
					else if(strcmp(line, "!AFC")==0)
						init_afc=false;
					else if(strcmp(line, "Au.SDLBUF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &sdlbuflen)!=1)
							{
								fprintf(stderr, "Bad Au.SDLBUF in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad Au.SDLBUF in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "Au.TXBUF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &txbuflen)!=1)
							{
								fprintf(stderr, "Bad Au.TXBUF in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad Au.TXBUF in conffile: no argument\n");
							return(1);
						}
					}
					else if(strcmp(line, "Au.RXBUF")==0)
					{
						if(colon)
						{
							if(sscanf(colon, "%u", &rxbuflen)!=1)
							{
								fprintf(stderr, "Bad Au.RXBUF in conffile: %s not numeric\n", colon);
								return(1);
							}
						}
						else
						{
							fprintf(stderr, "Bad Au.RXBUF in conffile: no argument\n");
							return(1);
						}
					}
					else
					{
						fprintf(stderr, "conffile: ignoring unrecognised line: %s:%s\n", line, colon);
					}
				}
				break;
			}
			free(line);
		}
		fclose(conffp);
	}
	else
	{
		fprintf(stderr, "No conffile.  Install one at ");
#ifdef WINDOWS
		fprintf(stderr, "3psk.conf\n");
#else // !WINDOWS
		fprintf(stderr, "~/.3psk\n");
#endif // WINDOWS
	}
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--txf=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &init_txf);
		}
		else if(strncmp(argv[arg], "--rxf=", 6)==0)
		{
			sscanf(argv[arg]+6, "%lg", &rxf);
			setrxf=true;
		}
		else if(strncmp(argv[arg], "--rxs=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &init_rxs);
		}
		else if(strncmp(argv[arg], "--if=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &aif);
		}
		else if(strncmp(argv[arg], "--amp=", 6)==0)
		{
			sscanf(argv[arg]+5, "%u", &init_amp);
		}
		else if(strncmp(argv[arg], "--txb=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &init_txb);
		}
		else if(strncmp(argv[arg], "--bw=", 5)==0)
		{
			unsigned int bw;
			sscanf(argv[arg]+5, "%u", &bw);
			unsigned int i;
			for(i=0;i<4;i++)
				if(bw==bandwidths[i])
					break;
			if(i<4)
				bws=i;
			else
			{
				fprintf(stderr, "Bad --bw: invalid value %u\n", bw);
				fprintf(stderr, "\tValid values are: %u", bandwidths[0]);
				for(unsigned int i=1;i<4;i++)
					fprintf(stderr, ", %u", bandwidths[i]);
				fprintf(stderr, "\n");
				return(1);
			}
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
		else if(strncmp(argv[arg], "--rx=", 5)==0)
		{
			wavrx=argv[arg]+5;
		}
		else
		{
			fprintf(stderr, "Unrecognised arg `%s'\n", argv[arg]);
			return(1);
		}
	}
	if(!setrxf) rxf=init_txf;
	fprintf(stderr, "Constructing GUI\n");
	gui G;
	{
		int e;
		if((e=make_gui(&G, &bws)))
		{
			fprintf(stderr, "Failed to construct GUI\n");
			return(e);
		}
		if(G.tx) *G.tx=false; else {fprintf(stderr, "Error: G.tx is NULL\n"); return(1);}
		if(G.moni) *G.moni=init_moni; else {fprintf(stderr, "Error: G.moni is NULL\n"); return(1);}
		if(G.afc) *G.afc=init_afc; else {fprintf(stderr, "Error: G.afc is NULL\n"); return(1);}
		if(G.spl) *G.spl=!setrxf; else {fprintf(stderr, "Error: G.spl is NULL\n"); return(1);}
		if(G.txb)
		{
			if((e=setspinval(G.txb, init_txb)))
			{
				fprintf(stderr, "Failed to set TXB spinner\n");
				return(1);
			}
		}
		else {fprintf(stderr, "Error: G.txb is NULL\n"); return(1);}
		if(G.txf)
		{
			if((e=setspinval(G.txf, init_txf)))
			{
				fprintf(stderr, "Failed to set TXF spinner\n");
				return(1);
			}
		}
		else {fprintf(stderr, "Error: G.txf is NULL\n"); return(1);}
		if(G.rxf)
		{
			if((e=setspinval(G.rxf, rxf)))
			{
				fprintf(stderr, "Failed to set RXF spinner\n");
				return(1);
			}
		}
		else {fprintf(stderr, "Error: G.rxf is NULL\n"); return(1);}
		if(G.rxs)
		{
			if((e=setspinval(G.rxs, init_rxs)))
			{
				fprintf(stderr, "Failed to set RXS spinner\n");
				return(1);
			}
		}
		else {fprintf(stderr, "Error: G.rxs is NULL\n"); return(1);}
		if(G.amp)
		{
			if((e=setspinval(G.amp, init_amp)))
			{
				fprintf(stderr, "Failed to set AMP spinner\n");
				return(1);
			}
		}
		else {fprintf(stderr, "Error: G.amp is NULL\n"); return(1);}
		for(unsigned int i=0;i<NMACROS;i++)
			if(G.macro[i])
				snprintf(G.macro[i], MACROLEN, "%s", init_macro[i]);
	}
	
	unsigned int inp=NMACROS;
	
	fprintf(stderr, "Starting audio subsystem\n");
	FILE *rwf=NULL;
	if(wavrx) rwf=fopen(wavrx, "rb");
	audiobuf rxaud, txaud;
	if(init_audiorx(&rxaud, rxbuflen, sdlbuflen, rwf)) return(1);
	if(init_audiotx(&txaud, txbuflen, sdlbuflen)) return(1);
	
	fprintf(stderr, "Setting up decoder frontend\n");
	
	fprintf(stderr, "frontend: Audio sample rate: %u Hz\n", rxaud.srate);
	unsigned long blklen=floor(rxaud.srate/(double)bandwidths[bws]);
	double bw=rxaud.srate/(double)blklen;
	
	fprintf(stderr, "frontend: FFT block length: %lu samples\n", blklen);
	if(blklen>4095)
	{
		fprintf(stderr, "frontend: FFT block length too long.  Exiting\n");
		return(1);
	}
	fprintf(stderr, "frontend: Filter bandwidth is %g Hz\n", bw);
	unsigned long k = max(floor((aif * (double)blklen / (double)rxaud.srate)+0.5), 1);
	double truif=(k*rxaud.srate/(double)blklen);
	fprintf(stderr, "frontend: Actual IF: %g Hz\n", truif);
	fftw_complex *fftin=fftw_malloc(sizeof(fftw_complex)*floor(rxaud.srate/(double)bandwidths[0]));
	fftw_complex *fftout=fftw_malloc(sizeof(fftw_complex)*floor(rxaud.srate/(double)bandwidths[0]));
	unsigned long speclen=max(floor(rxaud.srate/5.0), 360), spechalf=(speclen>>1)+1;
	double spec_hpp=rxaud.srate/(double)speclen;
	double *specin=fftw_malloc(sizeof(double)*speclen);
	fftw_complex *specout=fftw_malloc(sizeof(fftw_complex)*spechalf);
	fftw_set_timelimit(2.0);
	fprintf(stderr, "frontend: Preparing FFT plans\n");
	fftw_plan p[4], sp_p;
	for(unsigned int i=0;i<4;i++)
		p[i]=fftw_plan_dft_1d(floor(rxaud.srate/(double)bandwidths[i]), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	sp_p=fftw_plan_dft_r2c_1d(speclen, specin, specout, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	fprintf(stderr, "frontend: Ready\n");
	
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
	
	bool bitbuf[BITBUFLEN];
	unsigned int bitbufp=0;
	
	double old_da[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		old_da[i]=0;
	double t_da=0;
	double b_da[2]={-1, 1};
	bool fch=false;
	unsigned int st_ptr=0;
	unsigned int symtime[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		symtime[i]=0;
	bool st_loop=false;
	bool crossed[2]={false, false};
	
	unsigned int spec_pt[8][160];
	for(unsigned int i=0;i<8;i++)
		for(unsigned int j=0;j<160;j++)
			spec_pt[i][j]=60;
	unsigned int spec_which=0;
	double rxdphi=0;
	
	//double txphi=0;
	bbuf txbits={0, NULL};
	unsigned int txsetp=0, txoldp=0;
	unsigned int txlead=0;
	
	int errupt=0;
	unsigned long t=0, txt=0, frame=0, lastflip=0;
	fprintf(stderr, "Starting main loop\n");
	while(!errupt)
	{
		int16_t si;
		bool havesi=false;
		if(G.moni&&*G.moni&&((G.tx&&*G.tx)||txlead))
			rxsample(&rxaud, NULL);
		else
			havesi=!rxsample(&rxaud, &si);
		bool havetx=cantx(&txaud);
		if(havetx)
		{
			if((G.tx&&*G.tx)||txlead)
			{
				if((int)((txt*txb(G))%txaud.srate)<txb(G))
				{
					txoldp=txsetp;
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
								*G.tx=false;
								txlead=max(txb(G)/2, 8);
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
				double frac=min(fmod(txt*txb(G)/(double)(txaud.srate), 1)*2.0, 1.0);
				/*double sweep=txb(G)*M_PI*2*sin(frac*M_PI)/(double)txaud.srate;
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
				double txmag=1;//cos(M_PI/3)/cos(fmod(txphi, M_PI*2/3.0)-M_PI/3.0);*/
				double ft=(txt*2*M_PI*txf(G)/txaud.srate);//+sin(t*128.0/(double)txaud.srate)*0.2;
				//double tx=cos(ft+txphi)*txmag/3.0;
				double tx=(cos(ft+txsetp*M_PI*2/3.0)*pow(frac, 1.5)+cos(ft+txoldp*M_PI*2/3.0)*pow(1-frac, 1.5))/3.0;
				/*static int16_t nn=1;
				if(!nn)
					nn=1;
				else if(!(txt&3))
					nn=(nn<<1)+(((nn>>10)&1)^((nn>>12)&1)^((nn>>13)&1)^((nn>>15)&1));*/
				//tx+=(nn-32767.5)/65536.0;
				txsample(&txaud, tx*(1<<16)*.8);
				txt++;
				if(G.moni&&*G.moni)
				{
					si=tx*(1<<16)*.6;
					havesi=true;
				}
			}
			else
				txsample(&txaud, 0);
		}
		if(havesi)
		{
			double sv=si/(double)(1<<16);
			double phi=(t*2*M_PI*(truif-rxf)/rxaud.srate)+rxdphi;
			fftin[t%blklen]=sv*(cos(phi)+I*sin(phi))*amp(G)/5.0;
			if(!(t%speclen))
			{
				fftw_execute(sp_p);
				SDL_FillRect(G.spectro_img, &(SDL_Rect){0, 0, 160, 60}, SDL_MapRGB(G.spectro_img->format, SPEC_BG.r, SPEC_BG.g, SPEC_BG.b));
				for(unsigned int h=1;h<9;h++)
				{
					unsigned int x=floor(h*100/spec_hpp)-20;
					line(G.spectro_img, x, 0, x, 59, (atg_colour){31, 31, 31, ATG_ALPHA_OPAQUE});
				}
				unsigned int rx=floor(rxf/spec_hpp)-20;
				line(G.spectro_img, rx, 0, rx, 59, (atg_colour){0, 47, 0, ATG_ALPHA_OPAQUE});
				unsigned int tx=floor(txf(G)/spec_hpp)-20;
				line(G.spectro_img, tx, 0, tx, 59, (atg_colour){63, 0, 0, ATG_ALPHA_OPAQUE});
				for(unsigned int j=0;j<160;j++)
				{
					for(unsigned int k=1;k<8;k++)
						pset(G.spectro_img, j, spec_pt[(spec_which+k)%8][j], (atg_colour){k*20, k*20, 0, ATG_ALPHA_OPAQUE});
					double mag=4*sqrt(cabs(specout[j+20]));
					pset(G.spectro_img, j, spec_pt[spec_which][j]=max(59-mag, 0), (atg_colour){255, 255, 0, ATG_ALPHA_OPAQUE});
				}
				spec_which=(spec_which+1)%8;
			}
			specin[t%speclen]=sv;
			t++;
			if(!(t%blklen))
			{
				static fftw_complex oldz=0;
				fftw_execute(p[bws]);
				int x,y;
				double oldphi=carg(oldz), newphi=carg(fftout[k]);
				if((0<oldphi)&&(oldphi<M_PI/3.0)&&(newphi>M_PI/3.0)) crossed[0]=true;
				if((0>oldphi)&&(oldphi>-M_PI/3.0)&&(newphi<-M_PI/3.0)) crossed[1]=true;
				ztoxy(points[frame%CONSDLEN], gsf, &x, &y);
				if(lined[frame%CONSDLEN]) line(G.constel_img, x, y, 60, 60, CONS_BG);
				pset(G.constel_img, x, y, CONS_BG);
				fftw_complex half=points[(frame+(CONSDLEN>>1))%CONSDLEN];
				ztoxy(half, gsf, &x, &y);
				atg_colour c=(cabs(half)>sens)?(atg_colour){0, 127, 0, ATG_ALPHA_OPAQUE}:(atg_colour){127, 0, 0, ATG_ALPHA_OPAQUE};
				if(lined[(frame+(CONSDLEN>>1))%CONSDLEN]) line(G.constel_img, x, y, 60, 60, (atg_colour){0, 95, 95, ATG_ALPHA_OPAQUE});
				pset(G.constel_img, x, y, c);
				ztoxy(points[frame%CONSDLEN]=fftout[k], gsf, &x, &y);
				bool green=cabs(fftout[k])>sens;
				bool enough=false;
				double da;
				double mda=fabs(da=carg(fftout[k]));
				enough=(mda>M_PI/3.0);//&&(mda<M_PI*0.83);
				fftw_complex dz=bws?fftout[k]-points[(frame+CONSDLEN-1)%CONSDLEN]:fftout[k]/points[(frame+CONSDLEN-1)%CONSDLEN];
				bool spd=bws?(cabs(dz)<cabs(fftout[k])*blklen*blklen/(exp2(((signed)rxs(G)-32)/4.0)*2e4)):(fabs(carg(dz))<blklen/(exp2(((signed)rxs(G)-32)/4.0)*2e2));
				oldz=fftout[k];
				if((lined[frame%CONSDLEN]=(green&&enough&&(fch||spd))))
				{
					rxdphi-=da;
					line(G.constel_img, x, y, 60, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
					line(G.phasing_img, 0, 60, G.phasing_img->w, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
					int py=60+(old_da[st_ptr]-M_PI*2/3.0)*60;
					line(G.phasing_img, st_ptr*G.phasing_img->w/PHASLEN, py, (st_ptr+1)*G.phasing_img->w/PHASLEN, py, PHAS_BG);
					old_da[st_ptr]=(da<0)?da+M_PI*4/3.0:da;
					double d_da=old_da[st_ptr]-M_PI*2/3.0;
					t_da+=d_da;
					py=60+d_da*60;
					b_da[0]=max(b_da[0], d_da);
					b_da[1]=min(b_da[1], d_da);
					line(G.phasing_img, st_ptr*G.phasing_img->w/PHASLEN, py, (st_ptr+1)*G.phasing_img->w/PHASLEN, py, (da>0)?(atg_colour){255, 191, 255, ATG_ALPHA_OPAQUE}:(atg_colour){255, 255, 127, ATG_ALPHA_OPAQUE});
					unsigned int dt=t-symtime[st_ptr];
					double baud=rxaud.srate*(st_loop?PHASLEN:st_ptr)/(double)dt;
					snprintf(G.bauds, 8, "RXB %03d", (int)floor(baud+.5));
					symtime[st_ptr]=t;
					st_ptr=(st_ptr+1)%PHASLEN;
					if(!st_ptr) st_loop=true;
					if(G.afc&&*G.afc)
					{
						if(crossed[(da>0)?1:0]&&!crossed[(da>0)?0:1])
						{
							double ch=4.0/da;
							rxf-=ch;
							setspinval(G.rxf, floor(rxf+.5));
						}
					}
					crossed[0]=crossed[1]=false;
					if(!st_ptr)
					{
						if(G.afc&&*G.afc)
						{
							double diff=b_da[0]-b_da[1];
							double ch=t_da*rxaud.srate/(double)(dt*M_PI*2.0);
							if(fabs(ch)>0.1&&fabs(diff)<0.5)
							{
								rxf+=ch;
								setspinval(G.rxf, floor(rxf+.5));
							}
						}
						t_da=0;
						b_da[0]=-1;
						b_da[1]=1;
					}
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
				pset(G.constel_img, x, y, green?(atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE}:(atg_colour){255, 0, 0, ATG_ALPHA_OPAQUE});
				frame++;
				if(t>lastflip)
				{
					lastflip+=SAMPLE_RATE/8;
					if(G.spl) *G.spl=(rxf!=txf(G));
					if(G.underrun[0]) *G.underrun[0]=txaud.underrun;
					if(G.underrun[1]) *G.underrun[1]=rxaud.underrun;
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
												txstart(8);
											break;
											case SDLK_F2:
												append_str(&G.inr, &G.inrl, &G.inri, G.macro[1]);
												txstart(8);
											break;
											case SDLK_F3:
												append_str(&G.inr, &G.inrl, &G.inri, G.macro[2]);
												txstart(8);
											break;
											case SDLK_F4:
												append_str(&G.inr, &G.inrl, &G.inri, G.macro[3]);
												txstart(8);
											break;
											case SDLK_F5:
												append_str(&G.inr, &G.inrl, &G.inri, G.macro[4]);
												txstart(8);
											break;
											case SDLK_F6:
												append_str(&G.inr, &G.inrl, &G.inri, G.macro[5]);
												txstart(8);
											break;
											case SDLK_F7:
												txstart(8);
											break;
											case SDLK_F8:
												if(G.tx) *G.tx=false;
												txlead=max(txb(G)/2, 8);
											break;
											case SDLK_ESCAPE:
												if(G.tx) *G.tx=false;
												txlead=0;
												G.inri=0;
											break;
											case SDLK_F9:
												rxf=txf(G);
												setspinval(G.rxf, floor(rxf+.5));
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
												setspinval(G.txf, min(max((click.pos.x+20)*spec_hpp, 200), 800));
												/* fallthrough */
											case ATG_MB_RIGHT:
												rxf=min(max((click.pos.x+20)*spec_hpp, 200), 800);
												setspinval(G.rxf, floor(rxf+.5));
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
									if(value.e==G.bw)
									{
										bw=bandwidths[value.value];
										blklen=floor(rxaud.srate/bw);
										bw=rxaud.srate/(double)blklen;
										fprintf(stderr, "frontend: new FFT block length: %lu samples\n", blklen);
										if(blklen>4095)
										{
											fprintf(stderr, "frontend: new FFT block length too long.  Exiting\n");
											return(1);
										}
										fprintf(stderr, "frontend: new filter bandwidth is %g Hz\n", bw);
										k=max(floor((aif * (double)blklen / (double)rxaud.srate)+0.5), 1);
										truif=(k*rxaud.srate/(double)blklen);
										fprintf(stderr, "frontend: new Actual IF: %g Hz\n", truif);
										gsf=250.0/(double)blklen;
										sens=(double)blklen/16.0;
										SDL_FillRect(G.constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(G.constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
										for(size_t i=0;i<CONSLEN;i++)
										{
											points[i]=0;
											lined[i]=false;
										}
										fprintf(stderr, "frontend: all ready to go with new bandwidth\n");
									}
									else if(value.e->userdata)
									{
										if(strcmp((const char *)value.e->userdata, "TXF")==0)
										{
											if(G.spl&&!*G.spl)
												setspinval(G.rxf, rxf=value.value);
										}
										else if(strcmp((const char *)value.e->userdata, "RXF")==0)
										{
											rxf=value.value;
										}
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
											txlead=max(txb(G)/((G.tx&&*G.tx)?1:2), 8);
										else if(strcmp((const char *)toggle.e->userdata, "SPL")==0)
										{
											if(!toggle.state)
												setspinval(G.rxf, rxf=txf(G));
										}
									}
								}
							break;
							default:
							break;
						}
					}
				}
			}
		}
		if(!(havetx||havesi))
			SLEEP;
	}
	fprintf(stderr, "\nShutting down audio subsystem\n");
	stop_audiorx(&rxaud);
	stop_audiotx(&txaud);
	return(0);
}

void ztoxy(fftw_complex z, double gsf, int *x, int *y)
{
	if(x) *x=creal(z)*gsf+60;
	if(y) *y=cimag(z)*gsf+60;
}

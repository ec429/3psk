#include <SDL.h>
#include <atg.h>

#define INLINELEN	80
#define INLINES		5
#define OUTLINELEN	80
#define OUTLINES	5
#define MACROLEN	80
#define NMACROS		6

typedef struct
{
	atg_canvas *canvas;
	SDL_Surface *g_constel_img, *g_phasing_img, *g_spectro_img;
	bool *g_tx, *g_moni, *g_afc, *g_spl;
	unsigned int *bwsel;
	atg_element *g_bw, *g_txb, *g_txf, *g_rxf, *g_rxs, *g_amp;
	char *g_bauds;
	char *intextleft[INLINES], *intextright[INLINES];
	char *inr, *ing; size_t inrl, inri, ingl, ingi;
	char *outtext[OUTLINES];
	atg_colour *mcol[NMACROS];
	char *macro[NMACROS];
	atg_element *mline[NMACROS];
}
gui;

#define CONS_BG	(atg_colour){31, 31, 15, ATG_ALPHA_OPAQUE}
#define PHAS_BG	(atg_colour){15, 31, 31, ATG_ALPHA_OPAQUE}
#define SPEC_BG	(atg_colour){ 7,  7, 23, ATG_ALPHA_OPAQUE}

extern const char *set_tbl[6];

int make_gui(gui *buf, unsigned int *bws);
int setspinval(atg_element *spinner, int value);

int pset(SDL_Surface *s, unsigned int x, unsigned int y, atg_colour c);
int line(SDL_Surface *s, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, atg_colour c);
atg_element *create_selector(unsigned int *sel); // TODO make this malloc its own sel

/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2013
	Licensed under the GNU GPL v3+
	
	ptt: Push To Talk control
*/

#include <stdbool.h>
#ifdef WINDOWS
/* dummy only, PTT not supported */
enum ptt_line
{
	PTT_LINE_NONE	= 0,
	PTT_LINE_RTS,
	PTT_LINE_DTR,
	PTT_LINE_BOTH,
};
#else /* !WINDOWS */
#include <sys/ioctl.h>

enum ptt_line
{
	PTT_LINE_NONE	= 0,
	PTT_LINE_RTS	= TIOCM_RTS,
	PTT_LINE_DTR	= TIOCM_DTR,
	PTT_LINE_BOTH	= TIOCM_RTS | TIOCM_DTR,
};
#endif /* !WINDOWS */

struct ptt_settings
{
	char *devpath;
	int devfd;
	bool inverted;
	enum ptt_line line;
};

int ptt_open(struct ptt_settings *set);
int ptt_set(bool tx, const struct ptt_settings *set);
int ptt_close(struct ptt_settings *set);

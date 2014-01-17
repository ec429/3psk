/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2013
	Licensed under the GNU GPL v3+
	
	ptt: Push To Talk control
*/

#include "ptt.h"

#include <stdio.h>
#ifdef WINDOWS
/* dummy definitions - PTT not supported */
int ptt_open(struct ptt_settings *set)
{
	if(!set)
		return(1);
	if(!set->line)
	{
		fprintf(stderr, "PTT: not configured\n");
		return(0);
	}
	fprintf(stderr, "PTT: not supported on Windows\n");
	return(0);
}

int ptt_set(__attribute__((unused)) bool tx, __attribute__((unused)) const struct ptt_settings *set)
{
	return(0);
}

int ptt_close(__attribute__((unused)) struct ptt_settings *set)
{
	return(0);
}

#else /* !WINDOWS */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int ptt_open(struct ptt_settings *set)
{
	if(!set)
		return(1);
	if(!set->line)
	{
		fprintf(stderr, "PTT: not configured\n");
		return(0);
	}
	if(set->devfd>=0)
		ptt_close(set);
	fprintf(stderr, "PTT: devpath=%s\n", set->devpath);
	if(!set->devpath)
	{
		set->devfd=-1;
		return(2);
	}
	set->devfd=open(set->devpath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	fprintf(stderr, "PTT: devfd=%d\n", set->devfd);
	if(set->devfd<0)
	{
		perror("PTT: open");
		return(3);
	}
	// try to clear both lines
	int arg=PTT_LINE_BOTH;
	if(ioctl(set->devfd, TIOCMBIC, &arg)<0)
		perror("PTT: ioctl");
	fprintf(stderr, "PTT: initialised Push To Talk\n");
	return(0);
}

int ptt_set(bool tx, const struct ptt_settings *set)
{
	if(!set)
		return(1);
	if(!set->line)
		return(0);
	if(set->devfd<0)
		return(2);
	/*int current;
	if(!ioctl(set->devfd, TIOCMGET, &current))
		fprintf(stderr, "PTT: Current is %x\n", current);*/
	bool ptt=set->inverted?!tx:tx;
	//fprintf(stderr, "PTT: Setting %x to %s\n", set->line, ptt?"true":"false");
	if(ioctl(set->devfd, ptt?TIOCMBIS:TIOCMBIC, &set->line)<0)
	{
		perror("PTT: ioctl");
		return(3);
	}
	return(0);
}

int ptt_close(struct ptt_settings *set)
{
	if(!set)
		return(1);
	if(!set->line)
		return(0);
	ptt_set(false, set); // leave it in RX
	if(set->devfd>=0)
		close(set->devfd);
	set->devfd=-1;
	return(0);
}
#endif /* !WINDOWS */

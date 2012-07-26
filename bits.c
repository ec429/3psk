/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	bits.c: provides helper functions
*/
#include "bits.h"

uint16_t uget16(unsigned char *buf)
{
	if(!buf) return(-1);
	return(buf[0]|(buf[1]<<8));
}

uint32_t uget32(unsigned char *buf)
{
	if(!buf) return(-1);
	return(buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24));
}

void uput16(unsigned char buf[2], uint16_t val)
{
	buf[0]=val&0xFF;
	buf[1]=(val>>8)&0xFF;
}

void uput32(unsigned char buf[4], uint32_t val)
{
	buf[0]=val&0xFF;
	buf[1]=(val>>8)&0xFF;
	buf[2]=(val>>16)&0xFF;
	buf[3]=(val>>24)&0xFF;
}

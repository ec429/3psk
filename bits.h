#pragma once
/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	bits.h: helper functions
*/
#include <stdio.h>
#include <stdint.h>

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

#define uget16s(buf)		uget16((unsigned char *)(buf))
#define uget32s(buf)		uget32((unsigned char *)(buf))
#define uput16s(buf,val)	uput16((unsigned char *)(buf), val)
#define uput32s(buf,val)	uput32((unsigned char *)(buf), val)

uint16_t uget16(unsigned char *buf); // reads a 16-bit little-endian integer
uint32_t uget32(unsigned char *buf); // reads a 32-bit little-endian integer
void uput16(unsigned char[2], uint16_t val);
void uput32(unsigned char[4], uint32_t val);

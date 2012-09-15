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

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

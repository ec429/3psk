#pragma once
/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	frontend.h: constants for, and about, the frontend
*/

#define bandwidths ((const unsigned int[4]){10, 50, 150, 750})

#define set_tbl_strings ((const char *[6]){\
"Baud Rates",\
"BW min max",\
"10   1   7",\
"50   1  32",\
"150  4  90",\
"750 40 400"})

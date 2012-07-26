/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	varicode.h: Varicode encoding & decoding
*/
#include <stdbool.h>

typedef struct
{
	size_t nbits;
	bool *data;
}
bbuf;

bbuf encode(const char *text);
char *decode(bbuf data, int *ubits);

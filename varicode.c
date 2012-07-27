/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	varicode.h: Varicode encoding & decoding
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "varicode.h"
#include "strbuf.h"

char * varicode[128]={
"1010101011", "1011011011", "1011101101", "1101110111", "1011101011", "1101011111", "1011101111", "1011111101",
"1011111111", "11101111",   "11101",      "1101101111", "1011011101", "11111",      "1101110101", "1110101011",
"1011110111", "1011110101", "1110101101", "1110101111", "1101011011", "1101101011", "1101101101", "1101010111",
"1101111011", "1101111101", "1110110111", "1101010101", "1101011101", "1110111011", "1011111011", "1101111111",
"1",          "111111111",  "101011111",  "111110101",  "111011011",  "1011010101", "1010111011", "101111111",
"11111011",   "11110111",   "101101111",  "111011111",  "1110101",    "110101",     "1010111",    "110101111",
"10110111",   "10111101",   "11101101",   "11111111",   "101110111",  "101011011",  "101101011",  "110101101",
"110101011",  "110110111",  "11110101",   "110111101",  "111101101",  "1010101",    "111010111",  "1010101111",
"1010111101", "1111101",    "11101011",   "10101101",   "10110101",   "1110111",    "11011011",   "11111101",
"101010101",  "1111111",    "111111101",  "101111101",  "11010111",   "10111011",   "11011101",   "10101011",
"11010101",   "111011101",  "10101111",   "1101111",    "1101101",    "101010111",  "110110101",  "101011101",
"101110101",  "101111011",  "1010101101", "111110111",  "111101111",  "111111011",  "1010111111", "101101101",
"1011011111", "1011",       "1011111",    "101111",     "101101",     "11",         "111101",     "1011011",
"101011",     "1101",       "111101011",  "10111111",   "11011",      "111011",     "1111",       "111",
"111111",     "110111111",  "10101",      "10111",      "101",        "110111",     "1111011",    "1101011",
"11011111",   "1011101",    "111010101",  "1010110111", "110111011",  "1010110101", "1011010111", "1110110101"};

bbuf encode(const char *text)
{
	bbuf rv={0, NULL};
	if(!text) return(rv);
	rv.data=malloc((15*(1+strlen(text)))*sizeof(bool));
	if(!rv.data) return(rv);
	for(size_t i=0;text[i];i++)
	{
		unsigned char c=text[i];
		if(c&0x80) continue; // 8-bit Varicode isn't supported because I can't find a spec.  [G3PLX] just says "in numerical order"; it isn't at all clear what that means
		const char *code=varicode[c];
		for(size_t j=0;code[j];j++)
		{
			rv.data[rv.nbits++]=(code[j]=='1');
		}
		rv.data[rv.nbits++]=false;
		rv.data[rv.nbits++]=false;
	}
	return(rv);
}

char *decode(bbuf data, int *ubits)
{
	size_t j=0, k=0, q=0;
	char txt[data.nbits];
	char *rv;size_t rvl,rvi;
	init_char(&rv, &rvl, &rvi);
	for(size_t i=0;i<data.nbits;i++)
	{
		if(data.data[i])
			k=1;
		if(k)
		{
			txt[q++]=data.data[i]?'1':'0';
			if(!data.data[i])
				j++;
			else
				j=0;
			if(j>1)
			{
				txt[q-2]=0;
				for(unsigned char l=0;l<128;l++)
				{
					if(strcmp(txt, varicode[l])==0)
					{
						if(!isprint(l))
						{
							if((l=='\n')||(l=='\t'))
								append_char(&rv, &rvl, &rvi, l);
							else
							{
								append_char(&rv, &rvl, &rvi, '\\');
								append_char(&rv, &rvl, &rvi, '0'+((l>>6)&7));
								append_char(&rv, &rvl, &rvi, '0'+((l>>3)&7));
								append_char(&rv, &rvl, &rvi, '0'+(l&7));
							}
						}
						else
							append_char(&rv, &rvl, &rvi, l);
					}
				}
				k=0;j=0;q=0;*ubits=i;
			}
		}
	}
	return(rv);
}

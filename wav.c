/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	wav.c: provides functions for reading/writing RIFF/WAVE files
*/

#include "wav.h"
#include <stdlib.h>
#include <string.h>
#include "bits.h"

int werrno;

int read_wh44(FILE *wf, wavhdr *wh)
{
	char *hdr=malloc(44);
	if(!hdr)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			perror("read_wh44 bail: hdr == NULL: malloc:");
		#endif
		return(1);
	}
	size_t hb=fread(hdr, 1, 44, wf);
	if(hb!=44)
	{
		werrno=WEEOF;
		#ifdef DEBUG
			#ifdef WINDOWS
				fprintf(stderr, "read_wh44 bail: hb == %lu != 44\n", (unsigned long)hb);
			#else
				fprintf(stderr, "read_wh44 bail: hb == %zu != 44\n", hb);
			#endif
		#endif
		return(1);
	}
	wavhdr lh;
	if(memcmp(hdr, "RIFF", 4))
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: chunk_id != \"RIFF\"\n");
		#endif
		return(1);
	}
	uint32_t chk_size=uget32s(hdr+4);
	if(memcmp(hdr+8, "WAVE", 4))
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: format != \"WAVE\"\n");
		#endif
		return(1);
	}
	if(memcmp(hdr+12, "fmt ", 4))
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: subchunk1_id != \"fmt \"\n");
		#endif
		return(1);
	}
	uint32_t sc1s;
	if((sc1s=uget32s(hdr+16))!=16)
	{
		werrno=WENOSYS;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: subchunk1_size == %lu != 16\n", (unsigned long)sc1s);
		#endif
		return(1);
	}
	uint16_t audio_format;
	if((audio_format=uget16s(hdr+20))!=1)
	{
		werrno=WENOSYS;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: audio_format == %lu != 1\n", (unsigned long)audio_format);
		#endif
		return(1);
	}
	if((lh.num_channels=uget16s(hdr+22))==0)
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: num_channels == 0\n");
		#endif
		return(1);
	}
	if((lh.sample_rate=uget32s(hdr+24))==0)
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: sample_rate == 0\n");
		#endif
		return(1);
	}
	uint32_t byte_rate=uget32s(hdr+28); // SampleRate * NumChannels * BitsPerSample/8
	lh.block_align=uget16s(hdr+32); // NumChannels * BitsPerSample/8
	lh.bits_per_sample=uget16s(hdr+34);
	#ifdef DEBUG
		if(byte_rate!=lh.sample_rate*lh.num_channels*lh.bits_per_sample/8)
		{
			fprintf(stderr, "read_wh44 warn: byte_rate == %lu != %lu\n", (unsigned long)byte_rate, (unsigned long)lh.sample_rate*lh.num_channels*lh.bits_per_sample/8);
		}
		if(lh.block_align!=lh.num_channels*lh.bits_per_sample/8)
		{
			fprintf(stderr, "read_wh44 warn: block_align == %lu != %lu\n", (unsigned long)lh.block_align, (unsigned long)lh.num_channels*lh.bits_per_sample/8);
		}
	#else
		__attribute__((unused)) uint32_t dummy=byte_rate;
	#endif
	if(memcmp(hdr+36, "data", 4))
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: subchunk2_id != \"data\"\n");
		#endif
		return(1);
	}
	lh.data_len=uget32s(hdr+40);
	#ifdef DEBUG
		if(lh.data_len!=chk_size-36)
		{
			fprintf(stderr, "read_wh44 warn: data_len == %lu != %lu\n", (unsigned long)lh.data_len, (unsigned long)chk_size-36);
		}
	#else
		dummy=chk_size;
	#endif
	if((lh.data_len!=0xffffffff)&&(lh.data_len%lh.block_align))
	{
		werrno=WEDATA;
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 bail: data_len == %lu is not divisible by block_align == %lu\n", (unsigned long)lh.data_len, (unsigned long)lh.block_align);
		#endif
		return(1);
	}
	lh.num_samples=(lh.data_len==0xffffffff)?0xffffffff:lh.data_len/lh.block_align;
	if(wh)
		*wh=lh;
	else
	{
		#ifdef DEBUG
			fprintf(stderr, "read_wh44 warn: wh == NULL\n");
		#endif
	}
	return(0);
}

int write_wh44(FILE *wf, wavhdr wh)
{
	#ifdef DEBUG
		if(wh.bits_per_sample%8)
		{
			fprintf(stderr, "write_wh44 warn: bits_per_sample == %lu not divisible by 8\n", (unsigned long)wh.bits_per_sample);
		}
		if(wh.block_align!=wh.num_channels*wh.bits_per_sample/8)
		{
			fprintf(stderr, "write_wh44 warn: block_align == %lu != num_channels*bits_per_sample/8 == %lu\n", (unsigned long)wh.block_align, (unsigned long)wh.num_channels*wh.bits_per_sample/8);
		}
		// data_len * 8 / bits_per_sample
		if(wh.num_samples*wh.block_align!=wh.data_len)
		{
			fprintf(stderr, "write_wh44 warn: data_len == %lu != num_samples*block_align == %lu\n", (unsigned long)wh.data_len, (unsigned long)wh.num_samples*wh.block_align);
		}
	#endif
	if(fwrite("RIFF", 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	char buf[4];
	uput32s(buf, 36+wh.data_len);
	if(fwrite(buf, 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	if(fwrite("WAVEfmt ", 1, 8, wf)!=8)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput32s(buf, 16);
	if(fwrite(buf, 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput16s(buf, 1);
	if(fwrite(buf, 1, 2, wf)!=2)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput16s(buf, wh.num_channels);
	if(fwrite(buf, 1, 2, wf)!=2)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput32s(buf, wh.sample_rate);
	if(fwrite(buf, 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput32s(buf, wh.sample_rate*wh.block_align);
	if(fwrite(buf, 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput16s(buf, wh.block_align);
	if(fwrite(buf, 1, 2, wf)!=2)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput16s(buf, wh.bits_per_sample);
	if(fwrite(buf, 1, 2, wf)!=2)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	if(fwrite("data", 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	uput32s(buf, wh.data_len);
	if(fwrite(buf, 1, 4, wf)!=4)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_wh44 bail: write error\n");
		#endif
		return(1);
	}
	return(0);
}

long decode_sample(wavhdr wh, char *wd)
{
	if(!wd)
	{
		werrno=WEINVAL;
		#ifdef DEBUG
			fprintf(stderr, "decode_sample bail: wd == NULL\n");
		#endif
		return(0);
	}
	switch(wh.bits_per_sample)
	{
		case 8: // 8 bit (assume U8)
			return(*(unsigned char *)wd);
		/* no fallthrough */
		case 16:; // 16 bit (assume S16_LE)
			int16_t val=uget16s(wd); // XXX assumes the system uses twos' complement.  under C99 this conversion is implementation-defined & may raise a signal XXX
			return(val);
		/* no fallthrough */
		default:
			werrno=WENOSYS;
			#ifdef DEBUG
				fprintf(stderr, "decode_sample bail: unsupported bits_per_sample %lu\n", (unsigned long)wh.bits_per_sample);
			#endif
			return(0);
	}
}

int encode_sample(wavhdr wh, char *wd, long samp)
{
	if(!wd)
	{
		werrno=WEINVAL;
		#ifdef DEBUG
			fprintf(stderr, "encode_sample bail: wd == NULL\n");
		#endif
		return(1);
	}
	switch(wh.bits_per_sample)
	{
		case 8: // 8 bit (U8)
			wd[0]=samp;
		break;
		case 16: // 16 bit (S16_LE)
			uput16s(wd, samp);
		break;
		case 24: // 24 bit (S24_LE); not widely supported
			wd[0]=(samp>>16)&0xFF;
			uput16s(wd+1, samp&0xFFFF);
		break;
		case 32: // 32 bit (S32_LE)
			uput32s(wd, samp);
		break;
		default:
			werrno=WENOSYS;
			#ifdef DEBUG
				fprintf(stderr, "encode_sample bail: unsupported bits_per_sample %lu\n", (unsigned long)wh.bits_per_sample);
			#endif
			return(1);
	}
	return(0);
}

long read_sample(wavhdr wh, FILE *wf)
{
	char wd[wh.bits_per_sample>>3];
	size_t len=fread(wd, 1, wh.bits_per_sample>>3, wf);
	if(len!=(wh.bits_per_sample>>3))
	{
		werrno=WEEOF;
		#ifdef DEBUG
			fprintf(stderr, "read_sample bail: short fread\n");
		#endif
		return(0);
	}
	return(decode_sample(wh, wd));
}

int write_sample(wavhdr wh, FILE *wf, long samp)
{
	char wd[wh.bits_per_sample>>3];
	int e=encode_sample(wh, wd, samp);
	if(e) return(e);
	size_t len=fwrite(wd, 1, wh.bits_per_sample>>3, wf);
	if(len!=wh.bits_per_sample>>3)
	{
		werrno=WEERROR;
		#ifdef DEBUG
			fprintf(stderr, "write_sample: short fwrite\n");
		#endif
		return(1);
	}
	return(0);
}

long zeroval(wavhdr wh)
{
	switch(wh.bits_per_sample)
	{
		case 8: // 8 bit (U8)
			return(0x7f);
		break;
		case 16: // 16 bit (S16_LE)
		case 24: // 24 bit (S24_LE); not widely supported
		case 32: // 32 bit (S32_LE)
			return(0);
		break;
		default:
			werrno=WENOSYS;
			#ifdef DEBUG
				fprintf(stderr, "zeroval bail: unsupported bits_per_sample %lu\n", (unsigned long)wh.bits_per_sample);
			#endif
			return(0);
	}
}

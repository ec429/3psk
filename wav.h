#pragma once
/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
	
	wav.h: functions for reading/writing RIFF/WAVE files
*/
#include <stdio.h>
#include <stdint.h>
#define DEBUG

typedef struct
{
	uint16_t num_channels;
	uint32_t sample_rate; // samples per channel per second
	uint16_t block_align; // bytes per sample, = num_channels * bits_per_sample/8
	uint16_t bits_per_sample; // bits per sample per channel
	uint32_t data_len; // total length of data, in bytes
	uint32_t num_samples; // total length of data, in samples per channel, = data_len / block_align
}
wavhdr;

extern int werrno;

#define WEOK	0	// Success
#define WEERROR	1	// a system call failed, details in errno
#define WEINVAL	2	// Bad call parameter
#define WEEOF	3	// Unexpected end-of-file encountered (eg. fread() returned short count)
#define WEDATA	4	// Data error (eg. the supplied WAV file is corrupt)
#define WENOSYS	5	// Unimplemented (eg. the supplied WAV file does not use flat PCM encoding)
#define WEUNKNOWN 255	// Unknown error (eg. a function call failed but did not set werrno)

int read_wh44(FILE *, wavhdr *); // returns 0 on success
int write_wh44(FILE *, wavhdr); // returns 0 on success
long read_sample(wavhdr, FILE *); // returns (signed) sample value read
int write_sample(wavhdr, FILE *, long); // returns 0 on success
long zeroval(wavhdr); // returns the zero-value for the wav-format

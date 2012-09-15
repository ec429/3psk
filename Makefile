# Makefile for 3psk, 3-pole Phase Shift Keying
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=gnu99 -g `sdl-config --cflags`
LIBS := -lm -lfftw3 `sdl-config --libs` -lSDL_ttf -lSDL_audioin -latg
OBJS := bits.o varicode.o strbuf.o gui.o audio.o
INCLUDES := $(OBJS:.o=.h) frontend.h

all: 3psk

3psk: 3psk.c $(INCLUDES) $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(LIBS) $(OBJS) -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

varicode.o: strbuf.h

gui.o: frontend.h

audio.o: bits.h

clean:
	-rm 3psk *.o


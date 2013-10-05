# Makefile for 3psk, 3-pole Phase Shift Keying
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=gnu99 -g `sdl-config --cflags`
LIBS := -lm -lfftw3 `sdl-config --libs` -lSDL_ttf -lSDL_audioin -latg
OBJS := bits.o varicode.o strbuf.o gui.o audio.o ptt.o # wavlib/wavlib.o wavlib/bits.o
INCLUDES := $(OBJS:.o=.h) frontend.h
VERSION := `git describe --tags`

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

dist: all
	mkdir 3psk_$(VERSION)
	-for p in $$(ls); do cp $$p 3psk_$(VERSION)/$$p; done;
	-rm 3psk_$(VERSION)/*.tgz
	-rm 3psk_$(VERSION)/*.zip
	-rm 3psk_$(VERSION)/*.wav
	tar -czf 3psk_$(VERSION).tgz 3psk_$(VERSION)/
	rm -r 3psk_$(VERSION)/

dists: all
	mkdir 3psk_$(VERSION)_src
	-for p in *.c *.h Makefile *.htm screenshot.png sample.conf; do cp $$p 3psk_$(VERSION)_src/$$p; done;
	tar -czf 3psk_$(VERSION)_src.tgz 3psk_$(VERSION)_src/
	rm -r 3psk_$(VERSION)_src/

distw: all
	rm -rf 3psk_w$(VERSION)
	mkdir 3psk_w$(VERSION)
	-for p in $$(ls); do cp $$p 3psk_w$(VERSION)/$$p; done;
	-for p in $$(ls wbits); do cp wbits/$$p 3psk_w$(VERSION)/$$p; done;
	-rm 3psk_w$(VERSION)/*.tgz
	-rm 3psk_w$(VERSION)/*.zip
	-rm 3psk_w$(VERSION)/*.wav
	-rm 3psk_w$(VERSION)/*.o
	-rm 3psk_w$(VERSION)/3psk
	make -C 3psk_w$(VERSION) -fMakefile.w32 all
	-rm 3psk_w$(VERSION).zip
	zip -r 3psk_w$(VERSION).zip 3psk_w$(VERSION)
	rm -r 3psk_w$(VERSION)/


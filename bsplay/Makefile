PREFIX=/usr/local
CC=gcc
LD=$(CC)
CFLAGS=-g -O2 -Wall
LDFLAGS=
LDPLAYLIBS=-lavdevice -lavfilter -lswscale -lswresample -lavformat -lavcodec -lpostproc -lavutil -lasound -lSDL2 -lm

all: bsplay

bsplay: bsplay.c bsplay.h bsslist.o ffplay1.o
	$(CC) -I. -c $@.c -o $@.o $(CFLAGS) `pkg-config gtk+-2.0 --cflags`
	$(LD) $@.o bsslist.o -o $@ $(LDFLAGS) `pkg-config gtk+-2.0 --libs` ffplay1.o ../fftools/cmdutils.o $(LDPLAYLIBS)

ffplay1.o: ffplay1.c bsplay.h 
	$(CC) -I. -I../ -I../fftools -c ffplay1.c -o $@ $(CFLAGS) `pkg-config sdl2,glib-2.0,alsa,libass,libavformat --cflags`

bsslist.o: bsslist.c bsslist.h
	$(CC) -I. -c bsslist.c -o $@ $(CFLAGS)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -v bsplay $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -vm644 bsplay.desktop $(DESTDIR)$(PREFIX)/share/applications
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/16x16/apps
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/24x24/apps
	install -vm644 icons/16x16/* $(DESTDIR)$(PREFIX)/share/icons/hicolor/16x16/apps
	install -vm644 icons/24x24/* $(DESTDIR)$(PREFIX)/share/icons/hicolor/24x24/apps

install-strip: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -vs bsplay $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -vm644 bsplay.desktop $(DESTDIR)$(PREFIX)/share/applications
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/16x16/apps
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/24x24/apps
	install -vm644 icons/16x16/* $(DESTDIR)$(PREFIX)/share/icons/hicolor/16x16/apps
	install -vm644 icons/24x24/* $(DESTDIR)$(PREFIX)/share/icons/hicolor/24x24/apps

clean:
	rm -f *.o bsplay

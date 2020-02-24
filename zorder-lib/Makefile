# $Id: Makefile,v 1.3 2015/03/12 15:57:33 wes Exp $
# Version: $Name: Apr24-2015 $
# $Revision: 1.3 $

ZO_LIB=libzorder
ZO_TEST_PROGS = zo-sanity1 zo-sanity2 zo-sanity3 zo-sanity4

ALL:	$(ZO_LIB).a $(ZO_TEST_PROGS)

ZO_LIB_FILES=\
	zorder.c \
	zorder.h

ZO_LIB_SOURCES = zorder.c

ZO_LIB_OBJECTS = $(ZO_LIB_SOURCES:.c=.o)

PREFIX ?= /usr/local

CC = gcc
OPT = -g
CFLAGS = $(OPT) -I. -Wall
LDFLAGS = 

$(ZO_LIB).a:	$(ZO_LIB_OBJECTS)
	if (rm $(ZO_LIB).a) then :; fi
	ar q $(ZO_LIB).a $(ZO_LIB_OBJECTS)

.c.o:
	$(CC) -c $(CFLAGS) $<

zo-sanity1:	zo-sanity1.c $(ZO_LIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(ZO_LIB).a -lm -o zo-sanity1

zo-sanity2:	zo-sanity2.c $(ZO_LIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(ZO_LIB).a -lm -o zo-sanity2

zo-sanity3:	zo-sanity3.c $(ZO_LIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(ZO_LIB).a -lm -o zo-sanity3

zo-sanity4:	zo-sanity4.c $(ZO_LIB).a
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(ZO_LIB).a -lm -o zo-sanity4

clean:
	if (rm -f $(ZO_LIB_OBJECTS) *~ $(ZO_LIB).a $(ZO_TEST_PROGS)) then :; fi
	if (rm -rf *.dSYM) then :; fi

run-tests:	zo-sanity1 zo-sanity2 zo-sanity3 zo-sanity4
	@echo "running zorder sanity tests"
	@echo " "
	@echo zo-sanity1 -i 256 -j 256 -k 256
	./zo-sanity1 -i 256 -j 256 -k 256

	@echo " "
	@echo zo-sanity2 -i 256 -j 256 -k 256
	./zo-sanity2 -i 256 -j 256 -k 256

	@echo " "
	@echo zo-sanity4 -i 256 -j 256 -k 256
	./zo-sanity4 -i 256 -j 256 -k 256

# deps
zorder.o:	zorder.c zorder.h

install:	zorder.o $(ZO_LIB).a zorder.h
	install -d $(DESTDIR)$(PREFIX)/lib/
	install -m 644 zorder.o $(ZO_LIB).a $(DESTDIR)$(PREFIX)/lib/
	install -d $(DESTDIR)$(PREFIX)/include/
	install -m 644 zorder.h $(DESTDIR)$(PREFIX)/include/
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 zorder.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/

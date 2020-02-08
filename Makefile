LDLIBS = -lm -lzorder
CFLAGS = -std=gnu11 -Werror -Wall -Iinclude -I$(HOME)/include
LDFLAGS = -L$(HOME)/lib -Wl,-rpath,$(HOME)/lib

.PHONY: all
all: src/main test

src/main: src/fg.o

.PHONY: test
test: tests/test_fg
	for f in $^; do $$f; done

tests/%: CFLAGS += -Isrc -I$(HOME)/include
tests/%: LDFLAGS += -L$(HOME)/lib  -Wl,-rpath,$(HOME)/lib
tests/%: LDLIBS += -ltap

tests/test_fg: tests/common.o src/fg.o include/fg.h

SHELL := bash

CC = g++

CFLAGS += -D_GNU_SOURCE
CFLAGS += -g

ifneq ($(shell pkg-config --exists --print-errors zlib; echo $$?),0)
  $(error Missing Package: zlib)
endif
zlib_CFLAGS := $(shell pkg-config --cflags zlib)
zlib_LDLIBS := $(shell pkg-config --libs zlib)

m_CFLAGS :=
m_LDLIBS := -lm

dl_CFLAGS :=
dl_LDLIBS := -ldl -lEGL

INC:=-I/usr/include -Isrc
LIB:=-L/usr/lib/x86_64-linux-gnu

.PHONY: all
all: build/fg

.PHONY: clean
clean:

build:
	mkdir -p $@

build/MAB: | build
	mkdir -p $@

build/%.o: src/%.c | build
	$(CC) -x c++ $(CFLAGS) $(LIB) $(INC) -c -o $@ $<

build/MAB/%.o: src/MAB/%.c | build/MAB
	$(CC) $(CFLAGS) $(LIB) $(INC) -c -o $@ $<

build/%: build/%.o | build
	$(CC) $(CFLAGS) $(LIB) $(INC) -o $@ $^ $(LDLIBS) -Wl,-rpath=/usr/lib/x86_64-linux-gnu

src/shaders/%.h: src/shaders/%
	f=$<; \
	printf $$'static const char %s[] = {\n%s\n};' \
		"$${f//[^[:alnum:]]/_}" \
		"$$({ xxd -p $<; echo 00; } | xxd -p -r | xxd -i)" \
		> $@

build/base64.o:

build/fg.o:
build/fg.o: src/fg.h

build/glad.o: CFLAGS += $(dl_CFLAGS)
build/glad.o: src/glad/glad.h

build/glad_egl.o: CFLAGS += $(dl_CFLAGS)
build/glad_egl.o: src/glad/glad_egl.h

build/MAB/log.o: src/MAB/log.h

build/fg: CFLAGS += $(libmicrohttpd_CFLAGS) $(zlib_CFLAGS)
build/fg: LDLIBS += $(libmicrohttpd_LDLIBS) $(zlib_LDLIBS) $(egl_LDLIBS) $(dl_LDLIBS) $(m_LDLIBS) -pthread
build/fg: build/glad.o
build/fg: build/glad_egl.o
build/fg: build/base64.o

build/fg: LDLIBS += -luuid
build/fg: LDFLAGS += -pthread
build/fg:
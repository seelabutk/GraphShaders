SHELL := bash

CFLAGS += -D_GNU_SOURCE
CFLAGS += -g

ifneq ($(shell pkg-config --exists --print-errors libmicrohttpd; echo $$?),0)
  $(error Missing Package: libmicrohttpd)
endif
libmicrohttpd_CFLAGS := $(shell pkg-config --cflags libmicrohttpd)
libmicrohttpd_LDLIBS := -Wl,-rpath,$(shell pkg-config --variable=libdir libmicrohttpd) $(shell pkg-config --libs libmicrohttpd)

ifneq ($(shell pkg-config --exists --print-errors zlib; echo $$?),0)
  $(error Missing Package: zlib)
endif
zlib_CFLAGS := $(shell pkg-config --cflags zlib)
zlib_LDLIBS := $(shell pkg-config --libs zlib)

ifneq ($(shell pkg-config --exists --print-errors zorder; echo $$?),0)
  $(error Missing Package: zorder)
endif
zorder_CFLAGS := $(shell pkg-config --cflags zorder)
zorder_LDLIBS := $(shell pkg-config --libs zorder)

ifneq ($(shell pkg-config --exists --print-errors egl; echo $$?),0)
  $(error Missing Package: egl)
endif
egl_CFLAGS := $(shell pkg-config --cflags egl)
egl_LDLIBS := $(shell pkg-config --libs egl)

m_CFLAGS :=
m_LDLIBS := -lm

dl_CFLAGS :=
dl_LDLIBS := -ldl

.PHONY: all
all: build/server

.PHONY: clean
clean:

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/%: build/%.o | build
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

src/shaders/%.h: src/shaders/%
	f=$<; \
	printf $$'static const char %s[] = {\n%s\n};' \
		"$${f//[^[:alnum:]]/_}" \
		"$$({ xxd -p $<; echo 00; } | xxd -p -r | xxd -i)" \
		> $@

build/base64.o:

build/fg.o:

build/render.o: src/fg.h

build/server.o: CFLAGS += $(libmicrohttpd_CFLAGS)
build/server.o: src/server.h
build/server.o: src/shaders/default.vert.h
build/server.o: src/shaders/default.frag.h

build/glad.o: CFLAGS += $(dl_CFLAGS)
build/glad.o: src/glad/glad.h

build/server: CFLAGS += $(libmicrohttpd_CFLAGS) $(zlib_CFLAGS)
build/server: LDLIBS += $(libmicrohttpd_LDLIBS) $(zlib_LDLIBS) $(egl_LDLIBS) $(dl_LDLIBS) $(m_LDLIBS) $(zorder_LDLIBS)
build/server: build/fg.o
build/server: build/render.o
build/server: build/glad.o
build/server: build/base64.o

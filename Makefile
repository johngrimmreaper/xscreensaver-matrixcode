CC ?= cc
PKG_CONFIG ?= pkg-config
PYTHON ?= python3

CPPFLAGS ?=
CFLAGS ?= -O2 -g
LDFLAGS ?=
PROJECT_CPPFLAGS =

WARNINGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion \
           -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual \
           -Wwrite-strings -Wundef

X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags x11 2>/dev/null)
X11_LIBS := $(shell $(PKG_CONFIG) --libs x11 2>/dev/null)

HAVE_GL_HEADERS := $(shell test -f /usr/include/GL/gl.h -a -f /usr/include/GL/glx.h && echo yes)
ifeq ($(HAVE_GL_HEADERS),yes)
GL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gl 2>/dev/null)
GL_LIBS := $(shell $(PKG_CONFIG) --libs gl 2>/dev/null)
ifeq ($(strip $(GL_LIBS)),)
GL_LIBS := -lGL
endif
else
PROJECT_CPPFLAGS += -DMATRIXCODE_USE_MINIMAL_GL_HEADERS
GL_LIBS := -Wl,-l:libGL.so.1
endif

PREFIX ?= /usr
LIBEXECDIR ?= $(PREFIX)/libexec/xscreensaver
SYSCONFDIR ?= $(PREFIX)/share/xscreensaver/config
MANDIR ?= $(PREFIX)/share/man
APPLICATIONSDIR ?= $(PREFIX)/share/applications/screensavers
DESTDIR ?=

OBJECTS = matrixcode.o glyphs.o

all: matrixcode

matrixcode: $(OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) -o $@ $(OBJECTS) $(LDFLAGS) $(X11_LIBS) $(GL_LIBS) -lm

matrixcode.o: matrixcode.c glyphs.h compat/minigl.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(X11_CFLAGS) $(GL_CFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ matrixcode.c

glyphs.o: glyphs.c glyphs.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ glyphs.c

check: matrixcode
	./matrixcode -self-test
	./tests/test-cli.sh ./matrixcode
	./tests/test-xvfb.sh ./matrixcode

check-sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
	        LDFLAGS='-fsanitize=address,undefined' matrixcode
	./matrixcode -self-test

install: matrixcode
	install -D -m 0755 matrixcode $(DESTDIR)$(LIBEXECDIR)/matrixcode
	install -D -m 0644 config/matrixcode.xml $(DESTDIR)$(SYSCONFDIR)/matrixcode.xml
	install -D -m 0644 matrixcode.6x $(DESTDIR)$(MANDIR)/man6/matrixcode.6x
	install -D -m 0644 matrixcode.desktop $(DESTDIR)$(APPLICATIONSDIR)/matrixcode.desktop

clean:
	rm -f matrixcode $(OBJECTS) tests/matrixcode-test.ppm

.PHONY: all check check-sanitize install clean

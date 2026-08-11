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
HAVE_GL_HEADERS := $(shell test -f /usr/include/GL/gl.h && test -f /usr/include/GL/glx.h && echo yes)
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

NEWS_DATA = data/neo-workstation/search-results.json
NEWS_GENERATOR = tools/generate-neo-news.py
NEWS_GENERATED = generated/neo_news_seed.h

OBJECTS = matrixcode.o glyphs.o scene.o terminal_font.o neo_news.o neo_document.o neo_workstation.o

all: matrixcode
$(NEWS_GENERATED): $(NEWS_DATA) $(NEWS_GENERATOR) neo_news.h
	mkdir -p generated
	$(PYTHON) $(NEWS_GENERATOR) $(NEWS_DATA) $(NEWS_GENERATED)
matrixcode: $(OBJECTS)
	$(CC) $(CFLAGS) $(WARNINGS) -o $@ $(OBJECTS) $(LDFLAGS) $(X11_LIBS) $(GL_LIBS) -lm
matrixcode.o: matrixcode.c glyphs.h scene.h compat/minigl.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(X11_CFLAGS) $(GL_CFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ matrixcode.c
glyphs.o: glyphs.c glyphs.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ glyphs.c
scene.o: scene.c scene.h neo_workstation.h terminal_font.h compat/minigl.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(GL_CFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ scene.c
terminal_font.o: terminal_font.c terminal_font.h compat/minigl.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(GL_CFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ terminal_font.c
neo_news.o: neo_news.c neo_news.h $(NEWS_GENERATED)
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ neo_news.c
neo_document.o: neo_document.c neo_document.h neo_news.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ neo_document.c
neo_workstation.o: neo_workstation.c neo_workstation.h neo_news.h neo_document.h compat/minigl.h
	$(CC) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(GL_CFLAGS) $(CFLAGS) $(WARNINGS) -c -o $@ neo_workstation.c
check: matrixcode
	./matrixcode -self-test
	./tests/test-cli.sh ./matrixcode
	./tests/test-xvfb.sh ./matrixcode
check-sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' LDFLAGS='-fsanitize=address,undefined' matrixcode
	./matrixcode -self-test
install: matrixcode
	install -D -m 0755 matrixcode $(DESTDIR)$(LIBEXECDIR)/matrixcode
	install -D -m 0644 config/matrixcode.xml $(DESTDIR)$(SYSCONFDIR)/matrixcode.xml
	install -D -m 0644 matrixcode.6x $(DESTDIR)$(MANDIR)/man6/matrixcode.6x
	install -D -m 0644 matrixcode.desktop $(DESTDIR)$(APPLICATIONSDIR)/matrixcode.desktop
clean:
	rm -f matrixcode $(OBJECTS) $(NEWS_GENERATED) tests/*.ppm docs/*.ppm
.PHONY: all check check-sanitize install clean

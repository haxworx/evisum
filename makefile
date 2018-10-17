LIBS=

PREFIX= /usr/local

OSNAME := $(shell uname -s)

ifeq ($(OSNAME), OpenBSD)
	LIBS += -lkvm
	LDFLAGS += -I/usr/local/include -L/usr/local/lib -L/usr/X11R6/lib
endif

ifeq ($(OSNAME), FreeBSD)
        LDFLAGS += -L/usr/local/lib
endif

export PKGS = elementary

export LIBS

export LDFLAGS

default:
	$(MAKE) -C src
	$(MAKE) -C src/tingle

clean:
	$(MAKE) -C src/tingle clean

install:
	-mkdir -p $(PREFIX)/share/pixmaps
	-mkdir -p $(PREFIX)/share/applications
	-mkdir -p $(PREFIX)/bin
	install -m 0644 data/evisum.png $(PREFIX)/share/pixmaps
	install -m 0644 data/evisum.desktop $(PREFIX)/share/applications
	install -m 0755 evisum $(PREFIX)/bin
	install -m 0755 src/tingle/tingle $(PREFIX)/bin

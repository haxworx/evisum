LIBS=

PREFIX= /usr/local

OSNAME := $(shell uname -s)

ifeq ($(OSNAME), OpenBSD)
	LIBS += -lkvm
	LDFLAGS += -I/usr/local/include -L/usr/local/lib -L/usr/X11R6/lib
endif

export PKGS = elementary

export LIBS

export LDFLAGS

default:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean

install:
	cp data/evisum.png $(PREFIX)/share/icons
	cp data/evisum.desktop $(PREFIX)/share/applications
	cp evisum $(PREFIX)/bin

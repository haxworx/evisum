LIBS=

PREFIX= /usr/local

OSNAME := $(shell uname -s)

ifeq ($(OSNAME), OpenBSD)
	LIBS += -lkvm
	LDFLAGS += -I/usr/local/include -L/usr/local/lib -L/usr/X11R6/lib
endif

export CFLAGS = -g3 -ggdb3 -O0

export PKGS = eina elementary

export LIBS

export LDFLAGS

default:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean

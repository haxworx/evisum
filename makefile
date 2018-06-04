LIBS=

OSNAME := $(shell uname -s)

ifeq ($(OSNAME), OpenBSD)
	LIBS += -lkvm
	LDFLAGS += -I/usr/local/include -L/usr/local/lib -L/usr/X11R6/lib
endif

export CFLAGS = -g -ggdb3 -O

export PKGS = eina elementary

export LIBS

export LDFLAGS

default:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean

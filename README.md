# evisum

System Information (EFL)

This is a process monitor and system monitor.

Currently have full engines for Linux, FreeBSD, OpenBSD and MacOS.

The plan is to rewrite the whole program to log system information
over time to disk so we can do nice things with E and displaying
system information (not duplicating loads of code).

REQUIREMENTS:

An installation of EFL (v1.19.0+). Remember to set your PKG_CONFIG_PATH environment
variable accordingly. For example if EFL is installed in /opt:

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/libdata/pkgconfig"

BUILD:

$ make (or gmake)

INSTALL (default PREFIX is /usr/local):

$ make install

or as an example:

$ make PREFIX=/usr install

or even:

$ make PREFIX=/opt install

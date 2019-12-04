# evisum

System Information (EFL)

This is a process monitor and system monitor for Linux, macOS,
FreeBSD, DragonFlyBSD and OpenBSD.

REQUIREMENTS:

An installation of EFL (v1.19.0+). Remember to set your PKG_CONFIG_PATH environment variable accordingly. For example if EFL is installed in /opt:

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/libdata/pkgconfig"

BUILD:

$ meson build

$ ninja -C build

INSTALL:

$ ninja -C build install

NOTES

You can press 'k', 'm', 'g' to display results in KB, MB of GB
respectively.

The plan is to rewrite the whole program to log system information
over time to disk so we can do nice things with E and displaying
system information (not duplicating loads of code).

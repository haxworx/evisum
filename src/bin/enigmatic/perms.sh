#!/bin/sh

# We do this to allow us to poll for processes efficiently.
# Specifically for FreeBSD and DragonFlyBSD.
chown root:kmem "${DESTDIR}/${MESON_INSTALL_PREFIX}/bin/enigmatic"
chmod g+s "${DESTDIR}/${MESON_INSTALL_PREFIX}/bin/enigmatic"

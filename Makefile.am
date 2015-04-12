# ===============================================================================
# $Id: Makefile.am 960 2015-04-11 12:47:05Z ljp $
# Automake build script
# 
# ===============================================================================
AUTOMAKE_OPTIONS = gnu
SUBDIRS = . src docs

# This is needed to be able to install the systemd service in and absolute
# location and still have "make distcheck" work
DISTCHECK_CONFIGURE_FLAGS = --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

uninstall-local:
	rm -rf ${DESTDIR}${sysconfdir}/@PACKAGE@
	rm -rf ${DESTDIR}${prefix}/share/doc/packages/@PACKAGE@
	rm -rf ${DESTDIR}${prefix}/share/man/man1/g7ctrl.1*
	rm -rf ${DESTDIR}${prefix}/share/man/man1/g7sh.1*
	rm -rf ${DESTDIR}${prefix}/share/@PACKAGE@

EXTRA_DIST=assets NEWS.html
assetsdir=${prefix}/share/@PACKAGE@/assets
assets_DATA=assets/trainxbell.mp3 assets/dumpdb.sh


DISTCLEANFILES=.dep.inc



	
# ===============================================================================
# $Id: Makefile.am 227 2014-01-05 12:20:53Z ljp $
# Automake build script. This will create the utility library used to handle
# SMTP mail sending.
# ===============================================================================
AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' -pedantic -Wall -Werror -Wextra -Wpointer-arith \
-Wstrict-prototypes -Wno-error=unknown-pragmas

noinst_LIBRARIES = libsmtpmail.a
libsmtpmail_a_SOURCES = base64ed.c mailclientlib.c quotprinted.c base64ed.h mailclientlib.h quotprinted.h

EXTRA_DIST = README

CLEANFILES = *~

DISTCLEANFILES=*.Po


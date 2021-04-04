# ===============================================================================
# Create makefile for the unitbl library
# ===============================================================================
AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' -pedantic -Wall -Werror -Wextra -Wpointer-arith \
-Wstrict-prototypes -Wno-error=unknown-pragmas

noinst_LIBRARIES = libunitbl.a
libunitbl_a_SOURCES = unicode_tbl.c unicode_tbl.h

EXTRA_DIST = README 

CLEANFILES = *~

DISTCLEANFILES=*.Po

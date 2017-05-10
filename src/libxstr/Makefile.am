# ===============================================================================
# Create makefile for the xstr library
# ===============================================================================
AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' -pedantic -Wall -Werror -Wextra -Wpointer-arith \
-Wstrict-prototypes -Wno-error=unknown-pragmas

noinst_LIBRARIES = libxstr.a
libxstr_a_SOURCES = xstr.c xstr.h  assocarray.c assocarray.h

EXTRA_DIST = README 

CLEANFILES = *~

DISTCLEANFILES=*.Po
# ===============================================================================
# Create makefile for the table utility module
# ===============================================================================
AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' -pedantic -Wall -Werror -Wextra -Wpointer-arith \
-Wstrict-prototypes -Wno-error=unknown-pragmas

noinst_LIBRARIES = libhpdftbl.a
libhpdftbl_a_SOURCES = hpdf_errstr.c hpdf_grid.c hpdf_table.c hpdf_table_widget.c \
hpdf_table_widget.h hpdf_errstr.h hpdf_grid.h  hpdf_table.h  

EXTRA_DIST = README 

CLEANFILES = *~

DISTCLEANFILES=*.Po
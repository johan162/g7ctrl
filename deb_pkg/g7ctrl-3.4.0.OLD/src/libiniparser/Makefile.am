# ===============================================================================
# $Id$
# Automake build script. This will build our own copy of the iniparser library
# for systems who doesn't have a system version.
# ===============================================================================
AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' 

noinst_LIBRARIES = libiniparser.a
libiniparser_a_SOURCES = dictionary.c iniparser.c dictionary.h iniparser.h

EXTRA_DIST = README LICENSE INSTALL

CLEANFILES = *~

DISTCLEANFILES=*.Po







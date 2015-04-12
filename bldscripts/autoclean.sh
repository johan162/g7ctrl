#!/bin/sh
#
# Clean all files that autotools have generated
# They can be re-created by running bootstrap.sh
#
make maintainer-clean > /dev/null 2>&1
rm -f ChangeLog configure aclocal.m4 ar-lib config.guess config.rpath config.sub depcomp install-sh missing Makefile.in src/*.in src/libiniparser/*.in src/shell/*.in docs/*.in docs/manpages/*.in


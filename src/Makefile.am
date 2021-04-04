# ===============================================================================
# Automake build script
# Author:      Johan Persson (johan162@gmail.com)
#
# Copyright (C) 2014 Johan Persson
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>
# =========================================================================

AM_CFLAGS = -DCONFDIR='"$(sysconfdir)"' -pedantic -Wall -Werror -Wpointer-arith -Wstrict-prototypes \
-Wextra -Wshadow -Wno-error=unknown-pragmas -Werror=format -Wformat=2 -fstack-protector --param=ssp-buffer-size=4 -Wstack-protector \
`xml2-config --cflags` `curl-config --cflags` -D_FORTIFY_SOURCE=2

# The name of the daemon
bin_PROGRAMS = g7ctrl

# We construct the build number in a separate file which later is used to create a
# build number for the executable
BUILDNBR_FILE=buildnumber.txt

# Recurse into there directories
SUBDIRS = libiniparser libsmtpmail libhpdftbl libxstr libunitbl . shell etc


if DISABLE_PIE
    AM_CFLAGS += -fno-pie -no-pie
endif

# For development builds, as indicated by the stack trace flag, we want two
# additional features:
# 1) We need to include all symbols for the back trace to give proper function names
# 2) We also build the emulator
if STACKTRACE
    AM_CFLAGS += -rdynamic
    SUBDIRS += emul
endif

mailtemplatesdir=${prefix}/share/@PACKAGE@/mail_templates

dist_mailtemplates_DATA=mail_templates/mail_event.html  mail_templates/mail_lastloc.html  mail_templates/mail_tracker_conn.html \
mail_templates/mail_with_export_attachment.html mail_templates/mail_event.txt mail_templates/mail_lastloc.txt \
mail_templates/mail_tracker_conn.txt mail_templates/mail_with_export_attachment.txt \
mail_templates/mail_quotalimit.txt mail_templates/mail_quotalimit.html \
mail_templates/mail_event_img.html mail_templates/mail_event_img.txt \
mail_templates/mail_lastloc_img.html mail_templates/mail_lastloc_img.txt 

# All sources
g7ctrl_SOURCES = g7ctrl.c g7config.c futils.c utils.c lockfile.c logger.c pcredmalloc.c \
socklistener.c serial.c g7cmd.c tracker.c connwatcher.c dbcmd.c presets.c dict.c mailutil.c gpsdist.c \
g7srvcmd.c g7sendcmd.c sighandling.c nicks.c export.c geoloc.c wreply.c \
g7pdf_report_model.c g7pdf_report_view.c geoloc_cache.c \
g7ctrl.h g7config.h futils.h utils.h logger.h lockfile.h pcredmalloc.h build.h socklistener.h \
serial.h g7cmd.h tracker.h connwatcher.h dbcmd.h presets.h dict.h mailutil.h gpsdist.h \
g7srvcmd.h g7sendcmd.h sighandling.h nicks.h export.h geoloc.h wreply.h  \
g7pdf_report_model.h g7pdf_report_view.h geoloc_cache.h


# If we are using gcc then we construct the build number and date as "fake"
# symbols inserted directly to the linker. This is one way to assure that the
# uild number is always increased for each build even if no single file has changed.
# Unfortunately this onlye works with the gcc linker and not with clang
# therefor the config process figures out if the linker supports defining symbols
# and then sets the has_ld_defsym flag.
if has_ld_defsym
    g7ctrl_LDFLAGS = -Xlinker --defsym="__BUILD_NUMBER=$$(cat $(BUILDNBR_FILE))"
    g7ctrl_LDFLAGS += -Xlinker  --defsym="__BUILD_DATE=$$(date +'%y%m%d')"
endif

# If the system already have iniparser we use the system version. Otherwise we use our
# own version included with the daemon.
if have_iniparser
    g7ctrl_LDADD = $(LIBUSB_1_0) libsmtpmail/libsmtpmail.a libhpdftbl/libhpdftbl.a libxstr/libxstr.a libunitbl/libunitbl.a
    g7ctrl_DEPENDENCIES=$(BUILDNBR_FILE) libsmtpmail/libsmtpmail.a libhpdftbl/libhpdftbl.a libxstr/libxstr.a libunitbl/libunitbl.a
else
    g7ctrl_LDADD = $(LIBUSB_1_0) libsmtpmail/libsmtpmail.a libiniparser/libiniparser.a libhpdftbl/libhpdftbl.a libxstr/libxstr.a libunitbl/libunitbl.a
    g7ctrl_DEPENDENCIES=$(BUILDNBR_FILE) libsmtpmail/libsmtpmail.a libiniparser/libiniparser.a libhpdftbl/libhpdftbl.a libxstr/libxstr.a libunitbl/libunitbl.a
endif

# In Linux iconv() exists in glibc but in OSX we must add the iconv.dylib library to get that function
if is_osx
    g7ctrl_LDADD += /usr/lib/libiconv.2.dylib
endif


# In order to have access to the build variables and path settings in the the
# daemon we create an include file where we dump most of the config variables
# Listed in the same order as the GNU makefile conventions, and
# provided by autoconf 2.59c+ or 2.70.
# We use $(AM_V_GEN) to make the output less verbose when  silent rules have
# been enabled
configmake.h: Makefile
	$(AM_V_GEN)rm -f $@-t && \
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "$(prefix)"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DATADIR "$(datadir)"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define STATEDIR "/var/lib"'; \
	  echo '#define RUNSTATEDIR "$(runstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	  echo '#define SYSTEMDSYSTEMUNITDIR "$(systemdsystemunitdir)"'; \
	} | sed '/""/d' > $@-t && \
	mv -f $@-t $@

# Instruct automake what sources we are constructing on the fly so that the
# clean operations work as expected and we don't get any errors on distcheck.
BUILT_SOURCES=configmake.h

# Include the mail templates in the tar ball as well
EXTRA_DIST=mail_templates
DISTCLEANFILES=config.h $(BUILDNBR_FILE)

CLEANFILES=*~ g7ctrl configmake.h configmake.h-t

# Increase the build number each time we ruing make or initialize the build
# to one if it doesn't exist whenever an object file has been updated so that
# we have a new executable.
$(BUILDNBR_FILE): $(g7ctrl_OBJECTS)
	@if ! test -f $(BUILDNBR_FILE); then \
		echo 0 > $(BUILDNBR_FILE); \
	fi
	@echo $$(($$(cat $(BUILDNBR_FILE)) + 1)) > $(BUILDNBR_FILE)

# ===============================================================================
# $Id$
# Automake build script for gm7emul
#
# ===============================================================================
AM_CFLAGS = -DCONFDIR="\"$(sysconfdir)\"" -pedantic -Wall -Werror -Wextra -Wpointer-arith -Wstrict-prototypes -Wshadow -D_FORTIFY_SOURCE=2
bin_PROGRAMS = gm7emul
BUILDNBR_FILE=buildnumber.txt

gm7emul_SOURCES = gm7emul.c

if have_iniparser
gm7emul_LDADD=../libxstr/libxstr.a -lreadline
else
gm7emul_LDADD=../libxstr/libxstr.a  ../libiniparser/libiniparser.a -lreadline
endif

gm7emul_LDFLAGS =
if has_ld_defsym
gm7emul_LDFLAGS += -Xlinker --defsym -Xlinker "__BUILD_NUMBER=$$(cat $(BUILDNBR_FILE))"
gm7emul_LDFLAGS += -Xlinker --defsym -Xlinker "__BUILD_DATE=$$(date +'%y%m%d')"
endif

if has_opt_readline 
gm7emul_LDFLAGS += -L/usr/local/opt/readline/lib
gm7emul_CPPFLAGS = -I/usr/local/opt/readline/include
endif

EXTRA_DIST=README INSTALL

CLEANFILES=*~ gm7emul *.pid 

DISTCLEANFILES=$(BUILDNBR_FILE)

gm7emul_DEPENDENCIES=$(BUILDNBR_FILE)

$(BUILDNBR_FILE): $(gm7emul_OBJECTS)
	@if ! test -f $(BUILDNBR_FILE); then \
		echo 0 > $(BUILDNBR_FILE); \
	fi
	@echo $$(($$(cat $(BUILDNBR_FILE)) + 1)) > $(BUILDNBR_FILE)



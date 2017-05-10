# ===============================================================================
# Automake build script for g7sh
#
# ===============================================================================
AM_CFLAGS = -DCONFDIR="\"$(sysconfdir)\"" -pedantic -Wall -Werror -Wextra -Wpointer-arith -Wstrict-prototypes -Wshadow -D_FORTIFY_SOURCE=2
bin_PROGRAMS = g7sh
BUILDNBR_FILE=buildnumber.txt

g7sh_SOURCES = g7shell.c

if have_iniparser
g7sh_LDADD=../libxstr/libxstr.a -lreadline
else
g7sh_LDADD=../libxstr/libxstr.a ../libiniparser/libiniparser.a -lreadline
endif

g7sh_LDFLAGS =
if has_ld_defsym
g7sh_LDFLAGS += -Xlinker --defsym -Xlinker "__BUILD_NUMBER=$$(cat $(BUILDNBR_FILE))"
g7sh_LDFLAGS += -Xlinker --defsym -Xlinker "__BUILD_DATE=$$(date +'%y%m%d')"
endif

if has_opt_readline 
g7sh_LDFLAGS += -L/usr/local/opt/readline/lib
g7sh_CPPFLAGS = -I/usr/local/opt/readline/include
endif

EXTRA_DIST=README INSTALL

CLEANFILES=*~ tvpsh *.pid 

DISTCLEANFILES=$(BUILDNBR_FILE)

g7sh_DEPENDENCIES=$(BUILDNBR_FILE)

$(BUILDNBR_FILE): $(g7sh_OBJECTS)
	@if ! test -f $(BUILDNBR_FILE); then \
		echo 0 > $(BUILDNBR_FILE); \
	fi
	@echo $$(($$(cat $(BUILDNBR_FILE)) + 1)) > $(BUILDNBR_FILE)



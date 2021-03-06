# ===============================================================================
# Autoconf script for the g7ctrl daemon
# Use autoreconf to rebuild the build environment
# ===============================================================================
AC_PREREQ([2.69])
AC_INIT([g7ctrl],[3.6.0-beta],[johan162@gmail.com])
AC_COPYRIGHT([Part of configure script Copyright Johan Persson 2014-2021 (johan162@gmail.com)])
AC_PREFIX_DEFAULT(/usr)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([dist-xz silent-rules])
AM_SILENT_RULES
AM_PROG_AR
AC_PROG_LN_S
# AC_REQUIRE(AC_PROG_CC)
AC_CONFIG_HEADERS([src/config.h])


AC_CONFIG_FILES([
Makefile
src/Makefile
src/libiniparser/Makefile
src/libsmtpmail/Makefile
src/libhpdftbl/Makefile
src/libxstr/Makefile
src/libunitbl/Makefile
src/shell/Makefile
src/emul/Makefile
src/etc/g7ctrl.conf.template
src/etc/deb.init.d
src/etc/deb.init.conf
src/etc/ubuntu.init.d
src/etc/g7ctrl.service
src/etc/Makefile
docs/Makefile
docs/manpages/Makefile
docs/manual/Makefile
])


# ===============================================================================
# Set default sysconfdir to "/etc" in case prefix is not set by user
# We also check that the user hasn't specified sysconfdir directly
# ===============================================================================
AS_IF([ test "$prefix" = NONE],
    [test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc],
    AS_IF([ test x"$prefix" = "x/usr" -o x"$prefix" = "x/usr/local"],
          [test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc],
          [test "$sysconfdir" = '${prefix}/etc' && sysconfdir=${prefix}/etc] ))


# ===============================================================================
# Setup default directory defines for the daemon. These are generated into the
# "src/config.h" file.
# ===============================================================================
AS_IF([ test "$prefix" = NONE],
  [AC_DEFINE_UNQUOTED(DEFAULT_DATA_DIR,"/usr/share/$PACKAGE",[Default data directory for all assets])],
  [AC_DEFINE_UNQUOTED(DEFAULT_DATA_DIR,"$prefix/share/$PACKAGE",[Default data directory for all assets])] )

# ===============================================================================
# We add the AC_SUBST definition to be able to print the paths at the end of
# running configure
# ===============================================================================
AS_IF([ test "$prefix" = NONE],
  [AC_SUBST(DEFAULT_CONF_DATA_DIR,"/usr/share/$PACKAGE",[Default data directory for all assets])],
  [AC_SUBST(DEFAULT_CONF_DATA_DIR,"$prefix/share/$PACKAGE",[Default data directory for all assets])] )


# ===============================================================================
# If the final installation target is NONE or the usual "/usr" or "/usr/local"
# the the DB_DIR will be "/var/lib/@PACKAGE" as stipulated by LSB.
# For any other prefixes we assume that this is a stage build and really use the
# given prefix to control the final destination of DB_DIR
# ===============================================================================
AS_IF([ test "$prefix" = NONE],
  [AC_DEFINE_UNQUOTED(DEFAULT_DB_DIR,"/var/lib/$PACKAGE",[Default DB directory])],
  AS_IF([ test x"$prefix" = "x/usr" -o x"$prefix" = "x/usr/local"],
      [AC_DEFINE_UNQUOTED(DEFAULT_DB_DIR,"/var/lib/$PACKAGE",[Default DB directory])],
      [AC_DEFINE_UNQUOTED(DEFAULT_DB_DIR,"$prefix/var/lib/$PACKAGE",[Default DB directory])] ))

AS_IF([ test "$prefix" = NONE],
  [AC_SUBST(DEFAULT_CONF_DB_DIR,"/var/lib/$PACKAGE",[Default DB directory])],
  AS_IF([ test x"$prefix" = "x/usr" -o x"$prefix" = "x/usr/local"],
      [AC_SUBST(DEFAULT_CONF_DB_DIR,"/var/lib/$PACKAGE",[Default DB directory])],
      [AC_SUBST(DEFAULT_CONF_DB_DIR,"$prefix/var/lib/$PACKAGE",[Default DB directory])] ))


AC_DEFINE_UNQUOTED(DEFAULT_USER,"$PACKAGE",[Default user for running daemon])

# ===============================================================================
# Avoid warning from newer version of automake since the default is "cru"
# ===============================================================================
AC_SUBST(ARFLAGS,"cr")
AC_SUBST(AR_FLAGS,"cr")

# ===============================================================================
# Optional ARGS
#
# --enable-simulate   Allow the daemon to run on a server with no real connected
#                     device.
#
# --enable-stacktrace Force a stack trace to be written to /tmp/g7_stack.crash
#                     in case of a SIGSEGV or SIGBUS signal to help with debugging
# ===============================================================================
AC_ARG_ENABLE([simulate],
    [  --enable-simulate    Make daemon run on server without connected device],
    [enable_simulate=${enableval}],
    [enable_simulate=no])

AS_IF([ test "x${enable_simulate}" = xyes],
    [AC_DEFINE(DEBUG_SIMULATE,1,[Enable simulated connected device]) ] )

AC_ARG_ENABLE([stacktrace],
    [  --enable-stacktrace    Enable a stacktrace dump in case of SIGSEGV or SIGBUS error],
    [enable_stacktrace=${enableval}],
    [enable_stacktrace=no])

AC_ARG_ENABLE([pie],
    [  --disable-pie    Disable PIE code in executable. Creates old style ELF executable (non shared) ],
    [enable_pie=${enableval}],
    [enable_pie=yes])

AM_CONDITIONAL(DISABLE_PIE, [test "x${enable_pie}" = xno])

SYSTEM_NAME=`uname -s`
if test "x${enable_stacktrace}" = xyes; then
    if test x$SYSTEM_NAME = xDarwin; then
        AC_MSG_ERROR([Compiling with --enable-stacktrace is not supported when running on Darwin/OSX])
    else
        AC_DEFINE(SIGSEGV_HANDLER,1,[Enable a stacktrace dump in case of SIGSEGV or SIGBUS error])
   fi
fi

# This is needed so we can test for stracktrace build and in that case
# include the -rdynamic flag to gcc in order to have proper symbol names in
# the stacktrace
AM_CONDITIONAL(STACKTRACE, [test "x${enable_stacktrace}" = xyes])

# Add the system type (Linux or OS X) in config.h
# Also take the opportunity to add iconv as a separate lib on OSX
if test x$SYSTEM_NAME = xDarwin; then
    AC_DEFINE(IS_OSX,1,[True if system type is Apple OSX])
    AC_SUBST(DEFAULT_STTY_DEVICE,1411)
	# For OSX this is a separate library and we should check that it really works.
	# For Linux iconv is included in glibc-6 and is not a separate library we can check for
    AC_CHECK_LIB(iconv, iconv_open,,AC_MSG_ERROR([No iconv library found. Please install iconv (or similar)]))
else
    AC_DEFINE(IS_OSX,0,[True if system type is Apple OSX])
    AC_SUBST(DEFAULT_STTY_DEVICE,0)
fi

AM_CONDITIONAL(is_osx,[test x$SYSTEM_NAME = xDarwin])

# ===============================================================================
# Setup directory for systemd service file
#
# Optional ARGS
# --with-systemdsystemunitdir  Specify directory for systemd service script
# ===============================================================================
if test ! x$SYSTEM_NAME = xDarwin; then
    PKG_PROG_PKG_CONFIG
    AC_ARG_WITH([systemdsystemunitdir],
                AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
                [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
    if test "x$with_systemdsystemunitdir" != xno; then
        if test "x$with_systemdsystemunitdir" == x; then
            with_systemdsystemunitdir=/lib/systemd/system
        fi
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
    fi
    AC_DEFINE_UNQUOTED(SYSTEMD_INSTDIR,"$with_systemdsystemunitdir",[Where to install systemd service file])
    AM_CONDITIONAL(have_systemd, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])
else
    AM_CONDITIONAL(have_systemd,[test 0 = 1] )
fi

# ===============================================================================
# Setup directory for package documentation
# ===============================================================================
AS_IF([ test "$prefix" = NONE],
  [AC_SUBST([stdpackagedocdir], [${DESTDIR}/usr/share/doc/packages/${PACKAGE}])],
  [AC_SUBST([stdpackagedocdir], [${DESTDIR}${prefix}/share/doc/packages/${PACKAGE}])] )
AC_SUBST([stdpackagedochtmldir],[$stdpackagedocdir/manual])
AC_SUBST([stdpackagedochtmlimgdir],[$stdpackagedocdir/manual/img])


# ===============================================================================
# On some systems (e.g. OSX) inotify() does not exist
# We also use this as a "meta test" as whether the linker supports the --defsym
# argument or not but we define it as a separate automake variable in case we
# in the future finds a better way to test for this
# ===============================================================================
AM_CONDITIONAL(has_inotify, test -f /usr/include/sys/inotify.h -o -f /usr/include/linux/inotify.h)
AM_CONDITIONAL(has_ld_defsym, test -f /usr/include/sys/inotify.h -o -f /usr/include/linux/inotify.h)

# ===============================================================================
# OSX does not ship with readline and the BSD libedit which ships with OSX
# does not have all necessary functionality nor does it work very well. We
# therefore check if the user has installed the real readline()
# ===============================================================================
AC_DEFUN([AX_OSX_READLINE],[
AC_MSG_CHECKING([If it is OSX and GNU readline is installed])
OSX_GNU_READLINE=no
if test x$SYSTEM_NAME = xDarwin; then
    if test ! -d /usr/local/opt/readline; then
        AC_MSG_RESULT([no])
    else
        AC_MSG_RESULT([yes])
        OSX_GNU_READLINE=yes
    fi
else
    AC_MSG_RESULT([Not OSX])
fi
AM_CONDITIONAL(has_opt_readline, test -d /usr/local/opt/readline)
])


# ===============================================================================
# Standard checks for headers and C-compiler
# ===============================================================================
AC_LANG([C])
AC_PROG_INSTALL
AC_PROG_MAKE_SET

#AC_CHECK_HEADERS(libusb-1.0/libusb.h,,AC_MSG_ERROR([libusb.h not available. Please install "libusb-1.0-dev" package]))
AC_CHECK_HEADERS(readline/readline.h,[have_readline_h=yes],AC_MSG_ERROR([libreadline not available. Please install "libreadline-dev" ]))
AC_CHECK_HEADERS([arpa/inet.h fcntl.h stdlib.h string.h strings.h sys/param.h sys/socket.h sys/stat.h sys/ioctl.h syslog.h unistd.h])
have_iniparser_h=no
AC_CHECK_HEADERS(iniparser.h,[have_iniparser_h=yes],AC_MSG_NOTICE([iniparser.h not available. Will use built-in version]))
AC_CHECK_HEADERS(sys/inotify.h,[have_inotify_h=yes],AC_MSG_NOTICE([inotify.h not available. Will use pulling to detect USB connection]))
AC_CHECK_HEADERS(errno.h,,AC_MSG_ERROR([errno.h not available]))
AC_CHECK_HEADERS(syslog.h,,AC_MSG_ERROR([syslog.h not available]))
AC_CHECK_HEADERS(getopt.h,,AC_MSG_ERROR([getopt.h not available]))
AC_CHECK_HEADERS(signal.h,,AC_MSG_ERROR([signal.h not available]))
AC_CHECK_HEADERS(fcntl.h,,AC_MSG_ERROR([fcntl.h not available]))
AC_CHECK_HEADERS(pwd.h,,AC_MSG_ERROR([pwd.h not available]))
AC_CHECK_HEADERS(grp.h,,AC_MSG_ERROR([grp.h not available]))
AC_CHECK_HEADERS(libgen.h,,AC_MSG_ERROR([libgen.h not available]))
AC_CHECK_HEADERS(dirent.h,,AC_MSG_ERROR([dirent.h not available]))
AC_CHECK_HEADERS(pcre.h,,AC_MSG_ERROR([pcre.h not available. Please install "libpcre3-dev" package]))
AC_CHECK_HEADERS(sqlite3.h,,AC_MSG_ERROR([sqlite3.h not available. Please install "libsqlite3-dev" package]))

# ===============================================================================
# We need to determine which ranlib to use in order to build our static
# mail utility library
# ===============================================================================
AC_PROG_RANLIB

# ===============================================================================
# Checks for needed libraries. Note this will also append the correct linker flags
# to LDADD variable in the makefiles.
# ===============================================================================
AC_CHECK_LIB(pthread,pthread_create,,AC_MSG_ERROR([No pthread library found. Please install libpthread-dev (or similar)]))
if test "x${have_iniparser_h}" = xyes; then
   AC_CHECK_LIB(iniparser, iniparser_getstring,,AC_MSG_NOTICE([iniparser library seems invalid. Will use built-in ininparser]))
fi

AC_CHECK_LIB(m, lroundf,,AC_MSG_ERROR([No math library found. Please install libm-dev (or similar)]))
AC_CHECK_LIB(sqlite3, sqlite3_open,,AC_MSG_ERROR([No sqlite3 library found. Please install sqlite3-dev (or similar)]))
AC_CHECK_LIB(pcre, pcre_compile,,AC_MSG_ERROR([No pcre library found. Please install pcre-dev (or similar)]))
AC_CHECK_LIB(xml2, xmlStrcmp,,AC_MSG_ERROR([No xml2 library found. Please install libxml2-dev (or similar)]))
AC_CHECK_LIB(curl, curl_global_init,,AC_MSG_ERROR([No curl library found. Please install libcurl-dev (or similar)]))
AC_CHECK_LIB(hpdf, HPDF_New,,AC_MSG_ERROR([No Haru PDF library found. Please install libhpdf-dev (or similar)]))


# ===============================================================================
# Check for working fork and mktime functions just to be on the safe side
# ===============================================================================
AC_FUNC_FORK
AC_FUNC_MKTIME

# ===============================================================================
# Check for standard Unix types that we use. This is almost unnecessary since this
# should be available in all sane systems
# ===============================================================================
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# ===============================================================================
# Check for some standard functions that are used by the daemon
# ===============================================================================
AC_CHECK_FUNCS([atexit bzero floor getcwd memset mkdir rmdir select socket strchr strnlen strdup strerror clock_gettime])


# ===============================================================================
# MACRO AX_CHECK_XSLDOCBOOK
# The following macro will check to see if a working Docbook 5 environment can be found. It will also
# check for the presence of the fop program which is needed to rebuild the PDF versions of the man page.
# An end user of the library does not have to have the Docbook 5 environment installed since the distribution
# includes pre-built versions of the man pages. However a package maintainer that wants to modify or
# rebuild the man pages will need to have a full Docbook 5 setup.
# ===============================================================================
AC_DEFUN([AX_CHECK_XSLDOCBOOK],[
AC_CHECK_PROG(XSLTPROC,xsltproc,xsltproc,)
AC_CHECK_PROG(XMLCATALOG,xmlcatalog,xmlcatalog,)
AC_CHECK_PROG(XML2CONFIG,xml2-config,xml2-config,)
# AC_PATH_PROG (variable, prog-to-check-for, [value-if-not-found], [path])

XML_CATALOG=
for i in /etc/xml/catalog /usr/local/etc/xml/catalog do
    do
        if test -f "$i"; then
            XML_CATALOG_FILES=$i
            export XML_CATALOG_FILES
            break;
        fi
    done

XSLTPROC_WORKS=no
if test  "$XSLTPROC" -a "$XML2CONFIG" -a "$XMLCATALOG"; then
    AC_MSG_CHECKING([ for working docbook 5 ])
    DOCBOOK_XSL_SCHEMA="http://docbook.sourceforge.net/release/xsl-ns/current/html/docbook.xsl"
    $XSLTPROC --nonet $DOCBOOK_XSL_SCHEMA  << END
<?xml version="1.0" encoding="UTF-8"?>
<article xmlns="http://docbook.org/ns/docbook"
        xmlns:xi="http://www.w3.org/2001/XInclude"
        xmlns:xlink="http://www.w3.org/1999/xlink"
        version="5.0"
        >
</article>
END
	if test "$?" = 0; then
		XSLTPROC_WORKS=yes
                AC_MSG_RESULT([yes.])
        else
        	AC_MSG_RESULT([Docbook schema does not work. Check your catalog! ])
	fi
fi

AM_CONDITIONAL(have_xsltproc, test "$XSLTPROC_WORKS" = "yes")

AC_CHECK_PROG(LINKS_WWW,links,yes,no)
AM_CONDITIONAL(have_links, test "$LINKS_WWW" = "yes")

AC_CHECK_PROG(JAVAFOP,fop,yes,no)
AC_PATH_PROG(FOP,fop,fop,)
AM_CONDITIONAL(have_javafop, test "x$JAVAFOP" = "xyes")

if test "x$JAVAFOP" = "xyes"; then
    # Rudimentary test for major version of fop
    JAVAFOP_VERSION=2
    if test `fop -v 2> /dev/null | cut -c13` = 1 ; then
        JAVAFOP_VERSION=1        
    fi
    AM_CONDITIONAL(have_javafop2, test "x$JAVAFOP_VERSION" = "x2" ) 
fi

LOCAL_DOCBOOK_XSL_DIR=`xmlcatalog $XML_CATALOG_FILES http://docbook.sourceforge.net/release/xsl-ns/current/`
AC_SUBST(LOCAL_DOCBOOK_XSL_DIR)
])

# ===============================================================================
# MACRO AX_CHECK_DEBINIFILES
# Determine if the system uses Debian style ini files. This is the only
# suported start-up scripts suported 
# ===============================================================================
AC_DEFUN([AX_CHECK_DEBINIFILES],[
AC_MSG_CHECKING([If system uses debian style init.d])
if test ! -f /etc/rc.status; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([Systems not using Debian style init files are no longer supported (e.g. SuSE)])
fi
])



# ===============================================================================
# Generate an initial random password (6 characters)
# ===============================================================================
AC_DEFUN([AX_GEN_PWD],[

GENERATED_CLIENT_PWD=`tr -cd '[[:alnum:]]' < /dev/urandom | fold -w6 | head -n1`
AC_SUBST(GENERATED_CLIENT_PWD)

])

# ===============================================================================
# We need to define an automake conditional to know if we should use the built-in
# iniparser or as a system library.
# ===============================================================================
AM_CONDITIONAL(have_iniparser, test "x${have_iniparser_h}" = xyes)

# ===============================================================================
# Check if the system uses logrotate. In that case the automake will install
# a logrotate configuration file
# ===============================================================================
AM_CONDITIONAL(have_logrotate, test -d /etc/logrotate.d/)


# ===============================================================================
# Run all previous defined macros
# ===============================================================================
AX_CHECK_DEBINIFILES
AX_CHECK_XSLDOCBOOK
AX_GEN_PWD
AX_OSX_READLINE

# ===============================================================================
# Check what version of awk the system have
# ===============================================================================
AC_CHECK_PROG(HAVE_MAWK,mawk,yes,no)
AM_CONDITIONAL(have_mawk, test "$HAVE_MAWK" = "yes")
AC_PATH_PROG(MAWK,mawk,mawk,)

AC_CHECK_PROG(HAVE_GAWK,gawk,yes,no)
AM_CONDITIONAL(have_gawk, test "$HAVE_GAWK" = "yes")
AC_PATH_PROG(GAWK,gawk,gawk,)

# ===============================================================================
# We need cloc to regenerate source stats in the manual
# ===============================================================================
AC_CHECK_PROG(HAVE_CLOC,cloc,yes,no)
AM_CONDITIONAL(have_cloc, test "$HAVE_CLOC" = "yes")
AC_PATH_PROG(CLOC,cloc,cloc,)

# ===============================================================================
# Output all generated files
# ===============================================================================
AC_OUTPUT


# ===============================================================================
# Finally give some information to the user about the configuration setup
# ===============================================================================


AC_MSG_NOTICE([--------------------------------------------------------------------------------])
AC_MSG_NOTICE([INSTALLATION SUMMARY])

if test x$SYSTEM_NAME = xDarwin; then
    AC_MSG_NOTICE([ ])
    if test "$OSX_GNU_READLINE" = "yes"; then
        AC_MSG_NOTICE([  - System seems to be OSX and has the recommended GNU readline. ])
    else
        AC_MSG_NOTICE([  - System seems to be OSX but GNU readline does not seem to be installed.])
        AC_MSG_NOTICE([    Please install GNU readline to get full functionality for "g7sh"])
    fi
fi
AC_MSG_NOTICE([ ])
AC_MSG_NOTICE([  - Directory for config file: ${sysconfdir}/g7ctrl])
AC_MSG_NOTICE([  - Directory for static data files: "${DEFAULT_CONF_DATA_DIR}" ])
AC_MSG_NOTICE([  - Directory for dynamic data files: "${DEFAULT_CONF_DB_DIR}" ])
AC_MSG_NOTICE([ ])

if test "x${enable_stacktrace}" = xyes; then
    AC_MSG_NOTICE([  - Will install stacktrace handler for debug support ])
    AC_MSG_NOTICE([ ])
fi

if test -d /etc/logrotate.d ; then
    AC_MSG_NOTICE([  - Will install logrotate configuration file ])
    AC_MSG_NOTICE([ ])
fi

if  test "$XSLTPROC_WORKS" = "yes"; then
    if test "$JAVAFOP" = "no"; then
        AC_MSG_NOTICE([  - Will use limited Docbook5 setup (man- and HTML-pages can be rebuilt).])
        AC_MSG_NOTICE([    To also build PDF versions please install 'fop'])
    else
        AC_MSG_NOTICE([  - Working Docbook5 setup detected. Will rebuild documentation. ])
        AC_MSG_NOTICE([  - Using XML catalog "${XML_CATALOG_FILES}" ])
	AC_MSG_NOTICE([  - Docbook resolves to "${LOCAL_DOCBOOK_XSL_DIR}" ])
        AC_MSG_NOTICE([  - fop executable: ${FOP} (v${JAVAFOP_VERSION}) ])
        AC_MSG_NOTICE([    In case of errors rebuilding PDF regarding missing hyphenation])
        AC_MSG_NOTICE([    please refer to: http://offo.sourceforge.net/hyphenation/binary/installation.html ])
    fi
    if test "$LINKS_WWW" = "no"; then
        AC_MSG_NOTICE([  - Text browser "links" not found. Can not generate text version of manual])
    else
        AC_MSG_NOTICE([  - Text browser "links" found. Can generate text version of manual])
    fi

    if test "x${enable_pie}" = xno; then
        AC_MSG_NOTICE([  - Will disable PIE. Building old style (non shared) ELF executablels ])
        AC_MSG_NOTICE([ ])
    fi

else
    AC_MSG_NOTICE([  - Docbook5 setup NOT detected. Documentation can NOT be rebuilt.])
    AC_MSG_NOTICE([    In addition to the Docbooks stylesheets both "xsltproc" and "xml2-config"])
    AC_MSG_NOTICE([    must be in the path. Please check out the bootscript (bldscript directory)])
    AC_MSG_NOTICE([    to help setup a full environment if you need to re-render the documentation.])
fi

AC_MSG_NOTICE([--------------------------------------------------------------------------------])

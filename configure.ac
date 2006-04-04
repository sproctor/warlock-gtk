dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.58)

AC_INIT(warlock, 0.5-pre5)

dnl copyright
AC_COPYRIGHT([Copyright 2005 Sean Proctor <sproctor@gmail.com>
This configure script may be copied, distributed and modified under the 
terms of the Warlock license; see COPYING for more details])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AC_CANONICAL_HOST
AC_MSG_CHECKING([for Win32])
	case "$host" in
	*-*-mingw*)
	warlock_native_win32=yes
        WARLOCK_LIBS_EXTRA="$WARLOCK_LIBS_EXTRA -lwsock32"
	;;

	*)
	warlock_native_win32=no
        WARLOCK_LIBS_EXTRA="$WARLOCK_LIBS_EXTRA"
	;;
esac
AC_MSG_RESULT([$warlock_native_win32])
AC_SUBST(WARLOCK_LIBS_EXTRA)

if test x"$warlock_native_win32" = "xyes"; then
  AC_DEFINE(XP_WIN, [], [JS System type])
else
  AC_DEFINE(XP_UNIX, [], [JS system type])
fi

dnl Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

AM_PROG_LEX
if test "$LEX" != "flex"; then
  LEX="$SHELL $missing_dir/missing flex"
  AC_SUBST(LEX_OUTPUT_ROOT, lex.yy)
  AC_SUBST(LEXLIB, '')
fi

AC_PROG_YACC

dnl Checks for libraries.

dnl libgc needs dlopen on some systems for some odd reason
dnl AC_SEARCH_LIBS(dlopen, dl, [], [])

dnl check for boehm-gc
AC_CHECK_LIB(gc, GC_malloc, [],
  [AC_MSG_ERROR([You must have libgc installed. Please see \
http://www.hpl.hp.com/personal/Hans_Boehm/gc/ to download, or install the \
libgc developer package from your distribution (libgc-dev for Debian, gc-devel \
for Fedora)])])

dnl ===================================================================
dnl Check for SpiderMonkey, optional even if installed.
dnl ===================================================================

AC_ARG_WITH(spidermonkey, [  --without-spidermonkey  disable SpiderMonkey Mozilla JavaScript engine support],
            [if test "$withval" = no; then withval=""; disable_spidermonkey=yes; fi])
AC_MSG_CHECKING([for SpiderMonkey])

if test "$withval" = yes; then withval=""; fi

WARLOCK_SAVE_FLAGS
cf_result=no

if test -z "$disable_spidermonkey"; then
	if test ! -d "$withval"; then
		withval="";
	fi
	for spidermonkeydir in "$withval" "" /usr /usr/local /opt/spidermonkey /opt/js; do
		for spidermonkeyinclude in "/include" "/include/js" "/include/smjs"; do
			for spidermonkeylib in js smjs; do
				if test "$cf_result" = no; then
					SPIDERMONKEY_LIBS="-l$spidermonkeylib"

					if test ! -z "$spidermonkeydir"; then
						SPIDERMONKEY_LIBS="-L$spidermonkeydir/lib $SPIDERMONKEY_LIBS"
						SPIDERMONKEY_CFLAGS="-I$spidermonkeydir$spidermonkeyinclude"
					fi

					LIBS="$SPIDERMONKEY_LIBS $LIBS_X"
					CFLAGS="$CFLAGS_X $SPIDERMONKEY_CFLAGS"
					CPPFLAGS="$CPPFLAGS_X $SPIDERMONKEY_CFLAGS"

					AC_TRY_LINK([#define XP_UNIX
						     #include <jsapi.h>],
						     [JS_GetImplementationVersion()],
						     cf_result=yes, cf_result=no)
				fi
			done
		done
	done
fi

AC_MSG_RESULT($cf_result)

if test "$cf_result" != yes; then
        if test -z "$disable_spidermonkey"; then
                AC_MSG_ERROR("You must install SpiderMonkey with \
--enable-thread-safe (libsmjs-dev for Debian, Fedora \
and Gentoo will need to build the package from \
http://www.mozilla.org/js/spidermonkey/ because the \
versions that come with them are not thread-safe) or \
configure with --without-spidermonkey")
        else
		WARLOCK_RESTORE_FLAGS
        fi
else
	WARLOCK_CONFIG(CONFIG_SPIDERMONKEY, [SpiderMonkey])
fi

AM_CONDITIONAL(SPIDERMONKEY, test "$CONFIG_SPIDERMONKEY" = "yes")

AC_SUBST(SPIDERMONKEY_LIBS)
AC_SUBST(SPIDERMONKEY_CFLAGS)

dnl check for libpcre
AC_PATH_PROG(PCRE_CONFIG, pcre-config, no)
if test x"$PCRE_CONFIG" = "xno"; then
  AC_MSG_ERROR([You must have libpcre installed. Please see \
http://www.pcre.org/ to download, or install the libpcre developer package \
from your distribution (libpcre3-dev for Debian, pcre-devel for Fedora)])
fi
PCRE_CFLAGS=`pcre-config --cflags`
PCRE_LIBS=`pcre-config --libs`
AC_SUBST(PCRE_CFLAGS)
AC_SUBST(PCRE_LIBS)

dnl Check for GTK, etc
# Check return type of signal
AC_TYPE_SIGNAL

AC_CHECK_FUNCS(memmove strerror timegm)

PKG_CHECK_MODULES(EGG, gtk+-2.0 >= 2.5.0)
AC_SUBST(EGG_LIBS)
AC_SUBST(EGG_CFLAGS)
PKG_CHECK_MODULES(EGG_EXEC, libglade-2.0 libgnome-2.0 >= 2.0.0 gtk+-2.0 >= 2.0.0)
AC_SUBST(EGG_EXEC_LIBS)
AC_SUBST(EGG_EXEC_CFLAGS)
PKG_CHECK_MODULES(EGG_XML, libxml-2.0)
AC_SUBST(EGG_XML_LIBS)
AC_SUBST(EGG_XML_CFLAGS)

pkg_modules="gtk+-2.0 >= 2.6 gthread-2.0 libglade-2.0 gconf-2.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

dnl Check for GConf
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi
AM_GCONF_SOURCE_2

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`  
AC_SUBST(GLIB_GENMARSHAL)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

dnl Checks for header files.
dnl AC_PATH_X
dnl AC_HEADER_STDC
AC_CHECK_HEADERS([stdio.h stdlib.h string.h ctype.h unistd.h sys/types.h sys/stat.h], [],
  AC_MSG_ERROR([You need the above headers to compile Warlock.]))
AC_CHECK_HEADERS([netdb.h sys/socket.h], [], [
  AC_CHECK_HEADERS([winsock.h], [],
    [AC_MSG_ERROR([Couldn't find the required networking header(s).])])])

dnl Checks for typedefs, structures, and compiler characteristics.
dnl AC_C_CONST
dnl AC_TYPE_SIZE_T
dnl AC_STRUCT_TM

dnl Ensure MSVC-compatible struct packing convention is used when
dnl compiling for Win32 with gcc.
dnl What flag to use depends on gcc version: gcc3 uses "-mms-bitfields", while
dnl gcc2 uses "-fnative-struct".
if test x"$warlock_native_win32" = xyes; then
if test x"$GCC" = xyes; then
	msnative_struct=''
	AC_MSG_CHECKING([how to get MSVC-compatible struct packing])
	if test -z "$ac_cv_prog_CC"; then
		our_gcc="$CC"
	else
		our_gcc="$ac_cv_prog_CC"
	fi
	case `$our_gcc --version | sed -e 's,\..*,.,' -e q` in
	2.)
	if $our_gcc -v --help 2>/dev/null | grep fnative-struct >/dev/null; then
	msnative_struct='-fnative-struct'
	fi
	;;
	*)
	if $our_gcc -v --help 2>/dev/null | grep ms-bitfields >/dev/null; then
	msnative_struct='-mms-bitfields'
	fi
	;;

	esac

	if test x"$msnative_struct" = x ; then
	AC_MSG_RESULT([no way])
AC_MSG_WARN([produced libraries might be incompatible with MSVC-compiled code])
	else
	CFLAGS="$CFLAGS $msnative_struct"
AC_MSG_RESULT([${msnative_struct}])
	fi

fi
fi

dnl Checks for functions.
AC_CHECK_FUNCS([gethostbyname memset socket strstr])

dnl Options
dnl This breaks windows... so before we try to port to windows, make sure
dnl we don't sack the CFLAGS, or change the line above
if test x"$USE_MAINTAINER_MODE" = "xyes"; then
AC_DEFINE(DEBUG, 1, [enable debugging])
CFLAGS="-g -Wall -Wstrict-aliasing=2 -Wnested-externs -Wold-style-definition -Wdeclaration-after-statement -O0 -Werror"
AC_DEFINE(GTK_DISABLE_DEPRECATED, 1, [disable deprecated gtk functions])
else
CFLAGS="-O2"
fi

AC_OUTPUT(Makefile util/Makefile dock/Makefile src/Makefile)

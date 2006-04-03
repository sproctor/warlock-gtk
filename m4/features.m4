dnl ===================================================================
dnl Macros for various checks
dnl ===================================================================

dnl Save and restore the current build flags

dnl WARLOCK_CONFIG(define, what)
AC_DEFUN([WARLOCK_CONFIG], [
	  $1=yes
	  ABOUT_$1="$2"
	  AC_DEFINE($1, 1, [Define if you want: $2 support])])

AC_DEFUN([WARLOCK_SAVE_FLAGS],
[
	CFLAGS_X="$CFLAGS";
	CPPFLAGS_X="$CPPFLAGS";
	LDFLAGS_X="$LDFLAGS";
	LIBS_X="$LIBS";
])

AC_DEFUN([WARLOCK_RESTORE_FLAGS],
[
	CFLAGS="$CFLAGS_X";
	CPPFLAGS="$CPPFLAGS_X";
	LDFLAGS="$LDFLAGS_X";
	LIBS="$LIBS_X";
])

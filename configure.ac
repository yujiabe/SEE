dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_INIT
AM_INIT_AUTOMAKE(see, 1.0)
AM_CONFIG_HEADER(include/see/config.h)

dnl AC_SUBST(PACKAGE, [see])

AC_PROG_CC
AM_PROG_LIBTOOL

AC_STDC_HEADERS
AC_HEADER_TIME

dnl (some of the following headers used by alloca.c)
AC_CHECK_HEADERS([errno.h string.h stdlib.h],,,[;])

AC_TYPE_SIZE_T

AC_C_VOLATILE
AC_C_CONST
AC_C_BIGENDIAN

AC_CHECK_SIZEOF(signed int)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(signed short)
AC_CHECK_SIZEOF(unsigned short)
AC_CHECK_SIZEOF(signed long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(signed long long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MEMCMP

AC_LIBOBJ(vsnprintf)

AM_FUNC_STRTOD
AC_CHECK_FUNC(dtoa)
AC_CHECK_FUNC(freedtoa)
AC_CHECK_FUNC(strerror)

AC_CHECK_FUNC(isnan)
AC_CHECK_FUNC(finite)

AC_ARG_WITH(boehm-gc, 
    AC_HELP_STRING([--with-boehm-gc],
	[use Boehm-Weiser garbage collector (default yes)]),
	[ac_cv_use_boehmgc=$withval], [ac_cv_use_boehmgc=yes])
if x"$ac_cv_use_boehmgc" = x"yes"; then
    AC_CHECK_LIB(gc, gc_malloc, AC_MSG_ERROR("cannot use Boehm-gc"))
fi

AC_ARG_WITH(readline, 
    AC_HELP_STRING([--with-readline],
	[nicer UI with see-shell]),
	[ac_cv_use_readline=$withval], [ac_cv_use_readline=no])
if x"$ac_cv_use_readline" = x"yes"; then
    AC_CHECK_LIB(readline, readline, AC_MSG_ERROR("cannot use GNU readline"))
    AC_CHECK_HEADERS([readline/readline.h readline.h],,,[;])
fi

AC_ARG_WITH(debug,
    AC_HELP_STRING([--with-debug],
	[omit debug code and strings (default yes)]),
	[ac_cv_use_debug=$withval], [ac_cv_use_debug=yes])
if x"$ac_cv_use_debug" = x"no"; then
    AC_DEFINE(NDEBUG)
fi

AC_CHECK_TOOL(PERL, perl)
AC_SUBST(PERL)

AC_OUTPUT([Makefile include/Makefile include/see/Makefile libsee/Makefile shell/Makefile])


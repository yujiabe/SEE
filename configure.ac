dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_INIT(libsee/parse.c)
AM_INIT_AUTOMAKE(libsee, 1.0)
AM_CONFIG_HEADER(include/config.h)

AC_PROG_CC
AC_STDC_HEADERS

AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNC(vsnprintf, AC_DEFINE(HAVE_VSNPRINTF))

AC_CHECK_SIZEOF(signed int)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned short)
AC_CHECK_SIZEOF(signed long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

AC_C_VOLATILE

dnl -- for dtoa.c:
AC_C_BIGENDIAN
AC_C_CONST

AM_PROG_LIBTOOL

AC_OUTPUT([Makefile include/Makefile libsee/Makefile shell/Makefile])


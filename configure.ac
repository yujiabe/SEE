dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_INIT
AM_INIT_AUTOMAKE(libsee, 1.0)
AM_CONFIG_HEADER(include/see/config.h)

AC_SUBST(PACKAGE, [see])

AC_PROG_CC
AM_PROG_LIBTOOL

AC_STDC_HEADERS
AC_CHECK_HEADERS(unistd.h)

AC_TYPE_SIZE_T

AC_C_VOLATILE
AC_C_CONST
AC_C_BIGENDIAN

AC_CHECK_SIZEOF(signed int)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned short)
AC_CHECK_SIZEOF(signed long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STRTOD

AC_LIBOBJ(vsnprintf)

AC_ARG_WITH(boehm-gc, 
	      [  --with-boehm-gc         use Boehm-Weiser garbage collector], 
	      [AC_CHECK_LIB(gc, gc_malloc, AC_DEFINE(HAVE_GC))])

AC_ARG_WITH(readline, 
	      [  --with-readline         nicer UI with see-shell], 
	      [AC_CHECK_LIB(readline, readline, AC_DEFINE(HAVE_READLINE))])

AC_CHECK_TOOL(PERL, perl)
AC_SUBST(PERL)

AC_OUTPUT([Makefile include/Makefile include/see/Makefile libsee/Makefile shell/Makefile])


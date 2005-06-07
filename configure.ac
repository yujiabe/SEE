dnl Process this file with autoconf to produce a configure script.

AC_INIT([see],[1.3],[dleonard@users.sourceforge.net])
AC_PREREQ(2.52)
AC_REVISION([$Id$])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER(include/config.h)
AC_CONFIG_FILES([Makefile
		 include/Makefile
		 include/see/Makefile
		 libsee/Makefile 
		 libsee/libsee-config
		 doc/Makefile 
		 shell/Makefile
		 shell/test/Makefile])

dnl ------------------------------------------------------------
dnl programs required
dnl

AC_PROG_CC

AC_PROG_LIBTOOL

AC_ARG_VAR(PERL, [Practical Extraction and Report Language interpreter])
AC_PATH_PROG(PERL, perl)
m4_define(_perl_vers_var,$])
AC_CACHE_CHECK([$PERL version], [ac_cv_perl_version],
    [ac_cv_perl_version=`$PERL -e 'print _perl_vers_var;' || echo error`])
test "$ac_cv_perl_version" != error || \
    AC_MSG_ERROR([cannot find a working PERL interpreter])

dnl ------------------------------------------------------------
dnl C headers
dnl

AC_HEADER_STDC
AC_HEADER_TIME

dnl (alloca.h needs to know about these headers)
AC_CHECK_HEADERS([errno.h string.h stdlib.h],,,[;])

dnl (These make finding integer sizes simpler)
AC_CHECK_HEADERS([stdint.h inttypes.h],,,[;])

dnl (This is for some windows compat)
AC_CHECK_HEADERS([windows.h getopt.h],,,[;])

dnl ------------------------------------------------------------
dnl C compiler features
dnl

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
AC_CHECK_SIZEOF(signed LONGLONG)
AC_CHECK_SIZEOF(unsigned LONGLONG)

AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

AC_TYPE_SIZE_T

dnl ------------------------------------------------------------
dnl external libraries
dnl

AC_ARG_WITH(boehm-gc, 
    AC_HELP_STRING([--with-boehm-gc],
	[use Boehm-Weiser garbage collector, default=yes]),
	[ac_cv_use_boehmgc=$withval], [ac_cv_use_boehmgc=yes])
if test x"$ac_cv_use_boehmgc" != x"no"; then
    case "$ac_cv_use_boehmgc" in 
     yes|no) : ;;
     *) LDFLAGS="$LDFLAGS -L${ac_cv_use_boehmgc}/lib"
        CPPFLAGS="$CPPFLAGS -I${ac_cv_use_boehmgc}/include"
	;;
    esac
    AC_CHECK_HEADERS([gc.h],,,[;])
    AC_SEARCH_LIBS(GC_malloc, [gc])
fi

AC_ARG_WITH(readline, 
    AC_HELP_STRING([--with-readline],
	[see-shell interactive niceness, default=yes]),
	[ac_cv_use_readline=$withval], [ac_cv_use_readline=no])
if test x"$ac_cv_use_readline" != x"no"; then
    AC_CHECK_HEADERS([readline/readline.h readline.h],,,[;])
    AC_SEARCH_LIBS(tgoto, [termcap termlib curses ncurses])
    AC_SEARCH_LIBS(readline, [readline edit])
    AC_CHECK_FUNC(readline)
fi


dnl ------------------------------------------------------------
dnl std lib functions
dnl

AC_FUNC_ALLOCA
AC_FUNC_MEMCMP

AC_SEARCH_LIBS(sin, [m])		# the evil that lurks within

dnl -- pick the right nan/finite functions for the 64-bit float
if test $ac_cv_sizeof_float -eq 8; then
	AC_CHECK_FUNCS([isnanf finitef],,
		       [missing_funcs="$missing_funcs $ac_func"])
else
	AC_CHECK_FUNCS([isnan finite],,
		       [missing_funcs="$missing_funcs $ac_func"])
fi

dnl --- things to add later?
dnl  AC_CHECK_FUNCS([srand srandom vsprintf funopen])
dnl  AC_CHECK_FUNC(vsnprintf,,AC_LIBOBJ(vsnprintf))
dnl  AM_FUNC_STRTOD
dnl  AC_CHECK_FUNCS([dtoa freedtoa])

AC_CHECK_FUNC(rand_r)

dnl -- functions that we *need* (i.e. don't have workarounds for):
AC_CHECK_FUNCS([copysign rint \
		acos atan atan2 ceil cos exp floor log pow sin sqrt tan \
		snprintf vsnprintf \
		abort memcpy memset \
		qsort strcmp strerror],,
	       [missing_funcs="$missing_funcs $ac_func"])
 
if test -n "$missing_funcs"; then
    AC_MSG_ERROR([cannot find required functions:$missing_funcs])
fi

dnl -- functions that have workarounds written for
AC_CHECK_FUNCS([strdup GC_malloc getopt \
		gettimeofday GetSystemTimeAsFileTime time \
		localtime timegm
		])
AC_CHECK_FUNCS([gc_dump])

dnl ------------------------------------------------------------
dnl miscellanea
dnl

dnl
dnl no-return functions: helps the compiler optimise
dnl

AC_CACHE_CHECK([for attribute((noreturn))], [ac_cv_cc_attribute_noreturn],
	[AC_TRY_RUN([void f() __attribute__((__noreturn__));
		     void exit();
		     void f() { exit(0); for(;;); }
		     int main() { f(); exit(1); }],
		    [ac_cv_cc_attribute_noreturn=yes],
		    [ac_cv_cc_attribute_noreturn=no])])
test $ac_cv_cc_attribute_noreturn = yes && AC_DEFINE(HAVE_ATTRIBUTE_NORETURN)

dnl
dnl NaN and Infinity as inline expressions: another optimisation
dnl

AC_CACHE_CHECK([for constant NaN], [ac_cv_cc_constant_NaN_div],
	[AC_TRY_RUN([#include <math.h>
		     #if SIZEOF_FLOAT == 8
		     float nan = 0.0 / 0.0;
		     int main() { exit( isnanf(nan) && !finitef(nan) ? 0:1 ); }
		     #elif SIZEOF_DOUBLE == 8
		     double nan = 0.0 / 0.0;
		     int main() { exit( isnan(nan) && !finite(nan) ? 0:1 ); }
		     #else
		     # error "no 64-bit floating point type"
		     #endif
		     ],
		    [ac_cv_cc_constant_NaN_div=yes],
		    [ac_cv_cc_constant_NaN_div=no])])
test $ac_cv_cc_constant_NaN_div = yes && AC_DEFINE(HAVE_CONSTANT_NAN_DIV)

AC_CACHE_CHECK([for constant Inf], [ac_cv_cc_constant_Inf_div],
	[AC_TRY_RUN([#include <math.h>
		     #if SIZEOF_FLOAT == 8
		     float inf = 1.0 / 0.0;
		     int main() { exit( !isnanf(inf) && !finitef(inf) ? 0:1 ); }
		     #elif SIZEOF_DOUBLE == 8
		     double inf = 1.0 / 0.0;
		     int main() { exit( !isnan(inf) && !finite(inf) ? 0:1 ); }
		     #else
		     # error "no 64-bit floating point type"
		     #endif
		     ],
		    [ac_cv_cc_constant_Inf_div=yes],
		    [ac_cv_cc_constant_Inf_div=no])])
test $ac_cv_cc_constant_Inf_div = yes && AC_DEFINE(HAVE_CONSTANT_INF_DIV)

dnl
dnl Hexadecimal floating point constants are ANSI, but are not supported
dnl everywhere it seems.
dnl

AC_CACHE_CHECK([for hex float constants], [ac_cv_cc_constant_hex_float],
	[AC_TRY_RUN([#if SIZEOF_FLOAT == 8
		      float 
		     #elif SIZEOF_DOUBLE == 8
		      double
		     #else
		     # error "no 64-bit floating point type"
		     #endif
		        number = 0x1p3;
		     int main() { exit( number == 8 ? 0:1 ); }
		     ],
		    [ac_cv_cc_constant_hex_float=yes],
		    [ac_cv_cc_constant_hex_float=no])])
test $ac_cv_cc_constant_hex_float = yes && AC_DEFINE(HAVE_CONSTANT_HEX_FLOAT)

dnl
dnl GNU-style variadic macros
dnl

AC_CACHE_CHECK([for variadic macros], [ac_cv_cc_variadic_macros],
	[AC_TRY_RUN([
#define f(a, b...) g(a, 5 , ## b)
int g(v, w, x, y, z) int v,w,x,y,z; { return v*w*x*y*z; }
int main() { exit( f(2,3,7,9) == 2*3*5*7*9 ? 0 : 1 ); }
		     ],
		    [ac_cv_cc_variadic_macros=yes],
		    [ac_cv_cc_variadic_macros=no])])
test $ac_cv_cc_variadic_macros = yes && AC_DEFINE(HAVE_VARIADIC_MACROS)

dnl
dnl setjmp/longjmp
dnl

AC_CHECK_HEADERS([setjmp.h],,,[;])
AC_CACHE_CHECK([for working _longjmp()], [ac_cv_cc__longjmp_works],
	[AC_TRY_RUN([#include <setjmp.h>
		     int main () {
			jmp_buf b; volatile int c = 0;

			if (_setjmp(b) == 6) c++;
			if (c) exit(c == 2 ? 0 : c+1);
			c++;
			_longjmp(b, 6);
			exit(1);
		     }],
		    [ac_cv_cc__longjmp_works=yes],
		    [ac_cv_cc__longjmp_works=no])])
test $ac_cv_cc__longjmp_works = yes && AC_DEFINE(HAVE__LONGJMP)

AC_CACHE_CHECK([for working longjmp()], [ac_cv_cc_longjmp_works],
	[AC_TRY_RUN([#include <setjmp.h>
		     int main () {
			jmp_buf b; volatile int c = 0;

			if (setjmp(b) == 6) c++;
			if (c) exit(c == 2 ? 0 : c+1);
			c++;
			longjmp(b, 6);
			exit(1);
		     }],
		    [ac_cv_cc_longjmp_works=yes],
		    [ac_cv_cc_longjmp_works=no])])
test $ac_cv_cc_longjmp_works = yes && AC_DEFINE(HAVE_LONGJMP)

if test $ac_cv_cc__longjmp_works = no -a \
        $ac_cv_cc_longjmp_works = no; then
		AC_MSG_ERROR([cannot find working setjmp()/longjmp()])
fi

dnl
dnl check that vsnprintf works
dnl

AC_CACHE_CHECK([for working vsnprintf(0,0,,)], [ac_cv_libc_vsnprintf_works],
	[AC_TRY_RUN([#include <stdio.h>
		     #include <stdarg.h>

		     int a(fmt, ap)
			const char *fmt;
			va_list ap;
		     {
			return vsnprintf(0,0,fmt,ap);
		     }
		     int b(char *fmt, ...)
		     {
			va_list ap;
			int ret;
			va_start(ap, fmt);
			ret = a(fmt, ap);
			va_end(ap);
			return ret;
		     }
		     int main() {
			int ret = b("test %s", "string");
			exit( ret == 11 ? 0 : 1);
		     }],
		    [ac_cv_libc_vsnprintf_works=yes],
		    [ac_cv_libc_vsnprintf_works=no])])

dnl ------------------------------------------------------------
dnl IEEE doubles/floats
dnl

AC_CACHE_CHECK([for 64-bit IEEE754 fp], [ac_cv_cc_ieee754],
    [AC_TRY_RUN([
	int main() {
#if SIZEOF_FLOAT == 8
	    float 
#elif SIZEOF_DOUBLE == 8
	    double 
#else
# error "no 64-bit floating point type"
#endif
	      fp = 2.7182818284590452354;
	    unsigned char expected[8] = {
		0x40, 0x05, 0xbf, 0x0a, 0x8b, 0x14, 0x57, 0x69
	    }, e;
	    int i;

	    for (i = 0; i < 8; i++) {
#ifdef WORDS_BIGENDIAN
		e = expected[i];
#else
		e = expected[7-i];
#endif
		if (e != ((unsigned char *)&fp)[i])
		    exit(1);
	    }
	    exit(0);
	}],
    [ac_cv_cc_ieee754=yes],
    [ac_cv_cc_ieee754=no])])

if test x"$ac_cv_cc_ieee754" != x"yes"; then
    AC_MSG_WARN([SEE's ECMAScript compliance depends on native IEEE754 support])
fi

dnl ------------------------------------------------------------
dnl templates for autoheader
dnl

AH_TOP([#ifndef _SEE_h_config_
#define _SEE_h_config_])
AH_BOTTOM([#endif /* _SEE_h_config_ */])

AH_TEMPLATE([HAVE_ATTRIBUTE_NORETURN], 
	    [Define if your compiler understands __attribute__((__noreturn__))])
AH_TEMPLATE([HAVE_CONSTANT_INF_DIV],
	    [Define if your compiler treats the expression 1.0/0.0 as constant])
AH_TEMPLATE([HAVE_CONSTANT_NAN_DIV],
	    [Define if your compiler treats the expression 0.0/0.0 as constant])
AH_TEMPLATE([HAVE_CONSTANT_HEX_FLOAT],
	    [Define if your compiler handles ANSI hex fp constants like '0x1p3'])
AH_TEMPLATE([HAVE_VARIADIC_MACROS],
	    [Define if your preprocessor understands GNU-style variadic macros])
AH_TEMPLATE([HAVE_LONGJMP],
	    [Define if you have working longjmp() and setjmp() functions])
AH_TEMPLATE([HAVE__LONGJMP],
	    [Define if you have working _longjmp() and _setjmp() functions])

dnl ------------------------------------------------------------
dnl tests
dnl

AC_CONFIG_FILES([shell/test/test-grammar],
	  [chmod +x $ac_file])

dnl ------------------------------------------------------------
dnl finalisation
dnl

AC_OUTPUT

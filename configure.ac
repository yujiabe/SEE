dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_INIT
AM_INIT_AUTOMAKE(see, 1.0)
AM_CONFIG_HEADER(include/see/config.h)
AC_CONFIG_FILES([Makefile
		 include/Makefile
		 include/see/Makefile
		 libsee/Makefile 
		 shell/Makefile])

dnl ------------------------------------------------------------
dnl programs required
dnl

AC_PROG_CC

test x"$target" = x"" && target=NONE		# (workaround libtool bug)
AM_PROG_LIBTOOL

AC_CHECK_PROG(PERL, perl, [$ac_dir/$ac_word],
    [$PATH:/bin:/usr/bin:/usr/local/bin:/opt/local/bin])
AC_CACHE_CHECK([if $PERL works], [ac_cv_perl_works],
    [ac_cv_perl_works=no
     $PERL -e 'exit (0);' && ac_cv_perl_works=yes])
test $ac_cv_perl_works = yes || \
    AC_MSG_ERROR([a working perl interpreter is required])

dnl ------------------------------------------------------------
dnl C headers
dnl

AC_STDC_HEADERS
AC_HEADER_TIME

dnl (alloca.h needs to know about these headers)
AC_CHECK_HEADERS([errno.h string.h stdlib.h],,,[;])

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
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)

AC_TYPE_SIZE_T

dnl ------------------------------------------------------------
dnl std lib functions
dnl

AC_FUNC_ALLOCA
AC_FUNC_MALLOC
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
dnl  AC_CHECK_FUNC(rand_r,,AC_LIBOBJ(rand_r))
dnl  AM_FUNC_STRTOD
dnl  AC_CHECK_FUNCS([dtoa freedtoa])

dnl -- functions that we *need* (i.e. don't have workarounds for):
AC_CHECK_FUNCS([copysign rint \
		acos atan atan2 ceil cos exp floor log pow sin sqrt tan \
		snprintf vsnprintf rand_r \
		getopt \
		gettimeofday abort memcpy memset \
		qsort strcmp strerror],,
	       [missing_funcs="$missing_funcs $ac_func"])
 
if test -n "$missing_funcs"; then
    AC_MSG_ERROR([cannot find required functions:$missing_funcs])
fi

dnl -- functions that have workarounds written for
AC_CHECK_FUNCS([strdup])

dnl ------------------------------------------------------------
dnl external libraries
dnl

AC_ARG_WITH(boehm-gc, 
    AC_HELP_STRING([--with-boehm-gc],
	[use Boehm-Weiser garbage collector, default=yes]),
	[ac_cv_use_boehmgc=$withval], [ac_cv_use_boehmgc=yes])
AC_ARG_WITH(boehm-gc-libdir, 
    AC_HELP_STRING([--with-boehm-gc-libdir],
	[path to gc lib, default=PREFIX/lib]),
	[ac_cv_use_boehmgc_libdir=$withval],
	[if test x"$prefix" = x"NONE"; then
	     ac_cv_use_boehmgc_libdir=$ac_default_prefix/lib
	 else
	     ac_cv_use_boehmgc_libdir=$prefix/lib
	 fi])
AC_ARG_WITH(boehm-gc-incdir, 
    AC_HELP_STRING([--with-boehm-gc-incdir],
	[path to gc includes, default=PREFIX/include]),
	[ac_cv_use_boehmgc_incdir=$withval],
	[if test x"$prefix" = x"NONE"; then
	     ac_cv_use_boehmgc_incdir=$ac_default_prefix/include
	 else
	     ac_cv_use_boehmgc_incdir=$prefix/include
	 fi])
if test x"$ac_cv_use_boehmgc" != x"no"; then
    test x"$ac_cv_use_boehmgc_libdir" = x"no" || \
        LDFLAGS="$LDFLAGS -L${ac_cv_use_boehmgc_libdir}"
    test x"$ac_cv_use_boehmgc_incdir" = x"no" || \
        CPPFLAGS="$CPPFLAGS -I${ac_cv_use_boehmgc_incdir}"
    AC_CHECK_HEADERS([gc.h],,,[;])
    AC_SEARCH_LIBS(GC_malloc, [gc])
    AC_CHECK_FUNC(GC_malloc)
fi

AC_ARG_WITH(readline, 
    AC_HELP_STRING([--with-readline],
	[see-shell interactive niceness, default=yes]),
	[ac_cv_use_readline=$withval], [ac_cv_use_readline=no])
if test x"$ac_cv_use_readline" != x"no"; then
    AC_CHECK_HEADERS([readline/readline.h readline.h],,,[;])
    AC_SEARCH_LIBS(readline, [readline edit])
fi

dnl ------------------------------------------------------------
dnl debugging
dnl

AC_ARG_ENABLE(debug,
    AC_HELP_STRING([--enable-debug],
	[include debugging code, default=yes]),
	[ac_cv_enable_debug=$withval], [ac_cv_enable_debug=yes])
if test x"$ac_cv_enable_debug" = x"no"; then
    AC_DEFINE(NDEBUG)
fi

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
dnl setjmp/longjmp
dnl

AC_CHECK_HEADERS([setjmp.h],,,[;])
AC_CACHE_CHECK([working _longjmp()], [ac_cv_cc__longjmp_works],
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

AC_CACHE_CHECK([working longjmp()], [ac_cv_cc_longjmp_works],
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

dnl ------------------------------------------------------------
dnl templates for autoheader
dnl

AH_TOP([#ifndef _SEE_h_config_
#define _SEE_h_config_])
AH_BOTTOM([#endif /* _SEE_h_config_ */])

AH_TEMPLATE([NDEBUG], 
	    [Define to disable and remove all debugging code])
AH_TEMPLATE([HAVE_ATTRIBUTE_NORETURN], 
	    [Define if the compiler understands __attribute__((__noreturn__))])
AH_TEMPLATE([HAVE_CONSTANT_INF_DIV],
	    [Define if compiler can inline the expression 1.0/0.0 as constant])
AH_TEMPLATE([HAVE_CONSTANT_NAN_DIV],
	    [Define if compiler can inline the expression 0.0/0.0 as constant])
AH_TEMPLATE([HAVE_LONGJMP],
	    [Define if longjmp()/setjmp() are functional])
AH_TEMPLATE([HAVE__LONGJMP],
	    [Define if _longjmp()/_setjmp() are functional])

dnl ------------------------------------------------------------
dnl finalisation
dnl

dnl
dnl libtool+automake substitutions
dnl

AC_CONFIG_COMMANDS_PRE([LTLIBOBJS=`echo "$LIBOBJS" | sed 's/\.o/\.lo/g'`
			AC_SUBST(LTLIBOBJS)
			LTALLOCA=`echo "$ALLOCA" | sed 's/\.o/\.lo/g'`
			AC_SUBST(LTALLOCA)])

AC_OUTPUT


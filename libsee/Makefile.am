## Process this file with automake to produce Makefile.in
## $Id$

OBJEXT=		   @OBJEXT@

lib_LTLIBRARIES=   libsee.la
CLEANFILES=	   stringdefs.h stringdefs.inc
BUILT_SOURCES=	   stringdefs.h stringdefs.inc
libsee_la_SOURCES= cfunction.c scope.c debug.c dtoa.c dprint.c enumerate.c \
                   error.c function.c input_file.c input_lookahead.c	\
                   input_string.c input_utf8.c intern.c interpreter.c	\
                   lex.c mem.c native.c no.c obj_Array.c obj_Boolean.c	\
                   obj_Date.c obj_Error.c obj_Function.c obj_Global.c	\
                   obj_Math.c obj_Number.c obj_Object.c obj_RegExp.c	\
                   obj_String.c object.c parse.c printf.c regex.c 	\
		   string.c stringdefs.c system.c tokens.c try.c 	\
		   unicase.c unicode.c value.c version.c		\
		   module.c
		   
if PLATFORM_WIN32
libsee_la_SOURCES += platform_win32.c
endif
if PLATFORM_POSIX
libsee_la_SOURCES += platform_posix.c
endif

noinst_HEADERS=    array.h cfunction_private.h code.h dprint.h dtoa.h 	\
		   dtoa_config.h dprint.h enumerate.h function.h init.h \
		   lex.h nmath.h parse.h platform.h printf.h regex.h 	\
		   scope.h tokens.h unicase.inc unicode.h unicode.inc	\
		   stringdefs.h stringdefs.inc

libsee_la_LIBADD=  @LIBOBJS@
libsee_la_LDFLAGS= -version-info $(libsee_version_info)

EXTRA_DIST=	   string.pl string.defs
PERL=		   @PERL@

bin_SCRIPTS=	   libsee-config

INCLUDES=	   -I$(top_builddir)/include -I$(top_srcdir)/include

stringdefs.inc: string.defs string.pl
	$(PERL) $(srcdir)/string.pl c $(srcdir)/string.defs >$@
stringdefs.h: string.defs string.pl
	$(PERL) $(srcdir)/string.pl h $(srcdir)/string.defs >$@

#
# (From the libtool documentation:)
#
# 1. Start with version information of `0:0:0' for each libtool library.
# 
# 2. Update the version information only immediately before a public
#    release of your software.  More frequent updates are unnecessary,
#    and only guarantee that the current interface number gets larger
#    faster.
# 
# 3. If the library source code has changed at all since the last
#    update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
# 
# 4. If any interfaces have been added, removed, or changed since the
#    last update, increment CURRENT, and set REVISION to 0.
# 
# 5. If any interfaces have been added since the last public release,
#    then increment AGE.
# 
# 6. If any interfaces have been removed since the last public release,
#    then set AGE to 0.
#

libsee_version_info=	1:0:0

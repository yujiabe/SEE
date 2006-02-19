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
		   module.c platform_posix.c

noinst_HEADERS=    array.h cfunction_private.h dprint.h dtoa.h 		\
		   dtoa_config.h dprint.h enumerate.h function.h init.h \
		   lex.h nmath.h parse.h platform.h printf.h regex.h 	\
		   scope.h tokens.h unicase.inc unicode.h unicode.inc	\
		   stringdefs.h stringdefs.inc

libsee_la_LIBADD=  @LIBOBJS@

EXTRA_DIST=	   string.pl string.defs
PERL=		   @PERL@

bin_SCRIPTS=	   libsee-config

INCLUDES=	   -I$(top_builddir)/include -I$(top_srcdir)/include

stringdefs.inc: string.defs string.pl
	$(PERL) $(srcdir)/string.pl c $(srcdir)/string.defs >$@
stringdefs.h: string.defs string.pl
	$(PERL) $(srcdir)/string.pl h $(srcdir)/string.defs >$@

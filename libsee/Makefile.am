## Process this file with automake to produce Makefile.in
## $Id$

OBJEXT=		   @OBJEXT@

lib_LTLIBRARIES=   libsee.la
CLEANFILES=	   stringdefs.h stringdefs.inc
BUILT_SOURCES=	   stringdefs.h stringdefs.inc
libsee_la_SOURCES= cfunction.c context.c debug.c dtoa.c enumerate.c	\
                   error.c function.c input_file.c input_lookahead.c	\
                   input_string.c input_utf8.c intern.c interpreter.c	\
                   lex.c mem.c native.c no.c obj_Array.c obj_Boolean.c	\
                   obj_Date.c obj_Error.c obj_Function.c obj_Global.c	\
                   obj_Math.c obj_Number.c obj_Object.c obj_RegExp.c	\
                   obj_String.c object.c parse.c regex.c string.c 	\
                   stringdefs.c tokens.c try.c unicode.c value.c

#cfunction.lo: stringdefs.h stringdefs.inc

noinst_HEADERS=    array.h context.h dtoa.h dtoa_config.h enumerate.h	\
                   function.h lex.h parse.h regex.h tokens.h unicode.h	\
		   init.h

libsee_la_LIBADD=  @LIBOBJS@ @ALLOCA@ -lgc -lm

EXTRA_DIST=	   string.pl string.defs unicode.c
PERL=		   @PERL@

#-- the following DEFS/INCLUDES avoids including -I../include/see
#   yet allows us to keep config.h there, and stops <string.h> from
#   being an ambiguous #include.
#DEFS = 		   @DEFS@ -I. -I$(srcdir)
INCLUDES=	   -I$(top_builddir)/include -I$(top_srcdir)/include

stringdefs.inc: string.defs string.pl
	$(PERL) $(srcdir)/string.pl c $(srcdir)/string.defs >$@
stringdefs.h: string.defs string.pl
	$(PERL) $(srcdir)/string.pl h $(srcdir)/string.defs >$@
unicode.c:
#	cd $(top_builddir)/unicode && ${MAKE}
	cp $(top_srcdir)/unicode/unicode.c .

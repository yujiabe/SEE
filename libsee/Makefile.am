## Process this file with automake to produce Makefile.in
## $Id$

lib_LTLIBRARIES=   libsee.la

libsee_la_SOURCES= cfunction.c context.c debug.c dtoa.c enumerate.c	\
                   error.c function.c input_file.c input_lookahead.c	\
                   input_string.c input_utf8.c intern.c interpreter.c	\
                   lex.c mem.c native.c no.c obj_Array.c obj_Boolean.c	\
                   obj_Date.c obj_Error.c obj_Function.c obj_Global.c	\
                   obj_Math.c obj_Number.c obj_Object.c obj_RegExp.c	\
                   obj_String.c parse.c regex.c string.c stringdefs.c	\
                   tokens.c try.c value.c 				\
		   string.defs string.pl

noinst_HEADERS=    array.h context.h dtoa.h dtoa_config.h enumerate.h	\
                   function.h lex.h parse.h regex.h tokens.h unicode.h

libsee_la_LIBADD=  @LTLIBOBJS@ @LTALLOCA@ -lgc -lm

INCLUDES=	   -I$(srcdir)/../include

BUILT_SOURCES=	   unicode.c stringdefs.inc

PERL=		   @PERL@

stringdefs.inc: string.defs string.pl
	$(PERL) $(srcdir)/string.pl c $(srcdir)/string.defs >$@
stringdefs.h: string.defs string.pl
	$(PERL) $(srcdir)/string.pl h $(srcdir)/string.defs >$@

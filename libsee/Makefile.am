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
                   tokens.c try.c unicode.c value.c			

noinst_HEADERS=    array.h context.h dtoa.h dtoa_config.h enumerate.h	\
                   function.h lex.h parse.h regex.h tokens.h unicode.h

libsee_la_LIBADD=  @LTLIBOBJS@ @LTALLOCA@ -lgc -lm

INCLUDES=	   -I$(top_builddir)/include -I$(top_srcdir)/include

EXTRA_DIST=	   string.pl string.defs
BUILT_SOURCES=	   stringdefs.h stringdefs.inc
PERL=		   @PERL@

stringdefs.inc: string.defs string.pl
	$(PERL) $(srcdir)/string.pl c $(srcdir)/string.defs >$@
stringdefs.h: string.defs string.pl
	$(PERL) $(srcdir)/string.pl h $(srcdir)/string.defs >$@
unicode.c:
	@echo "unicode.c should be built prior to distribution!"
	cd $(top_builddir)/unicode && ${MAKE}

## Dependency rules to build stringdefs.h at the right time

stringdefs.o:: stringdefs.inc
cfunction.o error.o function.o intern.o lex.o native.o no.o obj_Array.o obj_Boolean.o obj_Date.o obj_Error.o obj_Function.o obj_Global.o obj_Math.o obj_Number.o obj_Object.o obj_RegExp.o obj_String.o parse.o regex.o string.o stringdefs.o tokens.o try.o value.o:: stringdefs.h 

stringdefs.lo:: stringdefs.inc
cfunction.lo error.lo function.lo intern.lo lex.lo native.lo no.lo obj_Array.lo obj_Boolean.lo obj_Date.lo obj_Error.lo obj_Function.lo obj_Global.lo obj_Math.lo obj_Number.lo obj_Object.lo obj_RegExp.lo obj_String.lo parse.lo regex.lo string.lo stringdefs.lo tokens.lo try.lo value.lo:: stringdefs.h 


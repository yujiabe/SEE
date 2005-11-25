## Process this file with automake to produce Makefile.in
## $Id$

bin_PROGRAMS=			see-shell
see_shell_SOURCES=		main.c shell.c getopt.c readline.c
noinst_HEADERS=			shell.h
see_shell_LDADD=		$(top_builddir)/libsee/libsee.la
see_shell_DEPENDENCIES=		$(top_builddir)/libsee/libsee.la

## A statically linked version for running 'make check'
noinst_PROGRAMS=		see-shell.static
see_shell_static_SOURCES=	$(see_shell_SOURCES)
see_shell_static_LDADD=		$(see_shell_LDADD)
see_shell_static_DEPENDENCIES=	$(see_shell_DEPENDENCIES)
see_shell_static_LDFLAGS=	 -static

#-- the following DEFS line tries to avoid the -I../include/see
DEFS = @DEFS@ -I. -I$(srcdir)
INCLUDES=			-I$(top_builddir)/include \
				-I$(top_srcdir)/include

EXTRA_DIST=			README

SUBDIRS=			test

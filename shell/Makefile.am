## Process this file with automake to produce Makefile.in
## $Id$

bin_PROGRAMS=		see-shell

see_shell_SOURCES=	main.c shell.c
noinst_HEADERS=		shell.h
see_shell_LDADD=	../libsee/libsee.la
see_shell_LDFLAGS=	-static
see_shell_DEPENDENCIES=	../libsee/libsee.la

INCLUDES=		-I$(top_srcdir)/include

SUBDIRS=		test

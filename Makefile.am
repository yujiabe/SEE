## Process this file with automake to produce Makefile.in
## $Id$

SUBDIRS = include libsee shell doc

EXTRA_DIST = AUTHORS COPYING INSTALL NEWS README TODO ChangeLog

ChangeLog:
	svn2cl --strip-prefix=see/trunk \
	    --include-rev --stdout $(srcdir) --revision=796:HEAD > $@ || \
	echo "No ChangeLog available" > $@

## -- test that installation allows a simple program to work
EXTRA_DIST += test1.c
installcheck:
	$(LIBTOOL) --mode=compile $(CC) -c \
	  `$(DESTDIR)$(bindir)/libsee-config --cppflags` \
	  $(srcdir)/test1.c -o test1.o
	$(LIBTOOL) --mode=link $(CC) \
	  `$(DESTDIR)$(bindir)/libsee-config --libs` \
	  test1.o -o test1
	$(LIBTOOL) --mode=execute ./test1

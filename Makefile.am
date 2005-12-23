## Process this file with automake to produce Makefile.in
## $Id$

## -- fix brokenness with automake-1.4
VERSION=@PACKAGE_VERSION@
PACKAGE=@PACKAGE_NAME@

SUBDIRS = include libsee shell doc

EXTRA_DIST = AUTHORS COPYING INSTALL NEWS README TODO ChangeLog

ChangeLog:
	svn2cl --strip-prefix=see/trunk \
	    --include-rev --stdout . --revision=796:HEAD > $@ || \
	echo "No ChangeLog available" > $@

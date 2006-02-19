#!/bin/sh
# $Id$
# Bootstrap the auxilliary files that don't need to be kept in CVS


#-- delete any previous cache
rm -rf autom4te.cache

#-- select the right autoconf tools 
if test x"$AUTOCONF_VERSION" = x""; then
	AUTOCONF_VERSION=2.59; export AUTOCONF_VERSION
	echo "export AUTOCONF_VERSION=$AUTOCONF_VERSION" >&2
fi
if test x"$AUTOMAKE_VERSION" = x""; then
	AUTOMAKE_VERSION=1.9; export AUTOMAKE_VERSION
	echo "export AUTOMAKE_VERSION=$AUTOMAKE_VERSION" >&2
fi

#M4=/usr/local/bin/gm4; export M4
set -ex
autoreconf --install --force

(cd unicode && make)

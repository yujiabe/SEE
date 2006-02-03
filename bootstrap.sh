#!/bin/sh
# $Id$
# Bootstrap the auxilliary files that don't need to be kept in CVS

set -ex

#-- delete any previous cache
rm -rf autom4te.cache

#-- select the right autoconf tools 
AUTOCONF_VERSION=2.59; export AUTOCONF_VERSION
AUTOMAKE_VERSION=1.9;  export AUTOMAKE_VERSION
#M4=/usr/local/bin/gm4; export M4
autoreconf --install --force

(cd unicode && make)

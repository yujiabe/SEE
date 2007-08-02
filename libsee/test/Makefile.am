## Process this file with automake to produce Makefile.in
## $Id: Makefile.am 1232 2007-06-01 13:18:48Z d $

#
# Running 'make check' will run the parser over a basic grammar
# that exercises every branch of the language grammar, a couple
# of the border cases of RegExp and some of the examples found in
# the ECMA-262 standard.
#
# Much better tests can be fashioned out of the overwhelmingly
# comprehensive Mozilla test suite. See:
#	http://www.mozilla.org/js/tests/library.html
#

AM_CPPFLAGS=	    -I$(srcdir) -I$(srcdir)/../../include -I../../include
AM_LDFLAGS=	    -L.. -lsee
noinst_PROGRAMS=    t-basic
TESTS=		$(noinst_PROGRAMS)
EXTRA_DIST=	common.inc

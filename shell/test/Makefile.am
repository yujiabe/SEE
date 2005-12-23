## Process this file with automake to produce Makefile.in
## $Id$

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

TESTS=		grammar.js regex.js function.js regress.js
TESTS_ENVIRONMENT= ../see-shell.static -f $(srcdir)/common.js -f
EXTRA_DIST=	common.js
SUBDIRS=
DIST_SUBDIRS=	mozilla

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

TESTS =	test

EXTRA_DIST=	test.sh grammar.js
EXTRA_CLEAN=	test

test: test.sh
	rm -f $@
	sed -e 's,=top_builddir=,$(top_builddir),g' \
	    -e 's,=srcdir=,$(srcdir),g' < $(srcdir)/test.sh > $@
	chmod +x $@

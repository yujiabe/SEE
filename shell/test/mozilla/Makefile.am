# $Id$

MOZROOT= :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
MOZROOTP= :pserver:anonymous:@cvs-mirror.mozilla.org:/cvsroot
MOZPASS= .cvspass
MOZDIR=	  mozilla.js.tests

SHELLDIR= $(shell cd ../.. && pwd)
SHELLPROG= $(SHELLDIR)/see-shell.static
SHELLOPTS= -c ext1 -c undefdef

TESTS=	run-mozilla-tests

check: $(MOZDIR)/Makefile $(SEESHELL)

$(MOZDIR)/Makefile: $(MOZPASS)
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOT) co \
		-d $(MOZDIR) mozilla/js/tests

$(MOZPASS):
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOTP) login

run-mozilla-tests: run-mozilla-tests.in
	sed -e 's!@SHELLPROG@!${SHELLPROG} ${SHELLOPTS}!g' \
	    -e 's!@MOZDIR@!${MOZDIR}!g' \
		< $^ > $@
	chmod +x $@

CLEANFILES=	run-mozilla-tests $(MOZPASS) run-mozilla-tests.out
extra-clean:
	rm -rf $(MOZDIR) $(MOZPASS) 

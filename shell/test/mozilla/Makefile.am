# $Id$

MOZROOT= :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
MOZROOTP= :pserver:anonymous:@cvs-mirror.mozilla.org:/cvsroot
MOZPASS= .cvspass
MOZDIR=	  mozilla.js.tests

SHELLDIR= $(shell cd ../.. && pwd)
SHELLPROG= $(SHELLDIR)/see-shell.static
SHELLOPTS= -f $(srcdir)/setup.js

TESTS=	run-mozilla-tests

check: $(MOZDIR)/Makefile $(SEESHELL)

$(MOZDIR)/Makefile: $(MOZPASS)
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOT) co \
		-d $(MOZDIR) mozilla/js/tests

$(MOZPASS):
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOTP) login

SUBST= sed -e 's!@SHELLPROG@!$(SHELLPROG)!g' \
	   -e 's!@SHELLOPTS@!$(SHELLOPTS)!g' \
	   -e 's!@MOZDIR@!$(MOZDIR)!g' \
	   -e 's!@srcdir@!$(srcdir)!g'

run-mozilla-tests: run-mozilla-tests.in
	$(SUBST) < $^ >$@
	chmod +x $@

CLEANFILES=	run-mozilla-tests $(MOZPASS) \
		run-mozilla-tests.out run-mozilla-tests.log
extra-clean:
	rm -rf $(MOZDIR) $(MOZPASS) 

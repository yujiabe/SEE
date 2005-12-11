# $Id$

MOZROOT= :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot
MOZROOTP= :pserver:anonymous:@cvs-mirror.mozilla.org:/cvsroot
MOZPASS= .cvspass
MOZDIR=	  mozilla.js.tests
SEESHELL= ../../see-shell.static
SEESHELLOPTS= -c ext1 -c undefdef

check: $(MOZDIR)/Makefile $(SEESHELL)

$(MOZDIR)/Makefile: $(MOZPASS)
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOT) co \
		-d $(MOZDIR) mozilla/js/tests

$(MOZPASS):
	CVS_PASSFILE=$(MOZPASS) cvs -d $(MOZROOTP) login

clean:
	rm -rf $(MOZDIR) $(MOZPASS)

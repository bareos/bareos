#
# Simple Makefile for building test plugins for Bareos
#

first_rule: all
dummy:

all: dir fd sd

dir:	dummy
	(cd dir && make DESTDIR=$(DESTDIR) || exit 1)
	@echo

fd:	dummy
	(cd fd && make DESTDIR=$(DESTDIR) || exit 1)
	@echo

sd:	dummy
	(cd sd && make DESTDIR=$(DESTDIR) || exit 1)
	@echo


clean:
	(cd dir && make DESTDIR=$(DESTDIR) clean || exit 1)
	(cd fd && make DESTDIR=$(DESTDIR) clean || exit 1)
	(cd sd && make DESTDIR=$(DESTDIR) clean || exit 1)

#
# Makefile for preparations of Debian builds
#

all: copyright

copyright: copyright.header copyright.footer
	@cat copyright.header > copyright
	@sed -r -e 's/^$$/./' -e 's/^[ ]*(.*)$$/    \1/' ../LICENSE >> copyright
	@cat copyright.footer >> copyright

clean:
	@rm -f *~ *.log
	@rm -f -r tmp/*

distclean: clean
	@find . -type f -a -name "*.in" -a ! -name "control.in" | sed -e 's/.in$$//' | xargs rm -f

devclean: clean

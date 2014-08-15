#
# Makefile for preparations of Debian builds
#

all: copyright

copyright: copyright.header copyright.footer
	@install copyright.header copyright
	@sed -r -e 's/^$$/./' -e 's/^[ ]*(.*)$$/    \1/' ../LICENSE >> copyright
	@cat copyright.footer >> copyright

clean:
	@rm -f *~ *.log
	@rm -f -r tmp/*

distclean: clean
	@find . -type f -a -name "*.in" | sed -e 's/.in$$//' | xargs rm -f
	# control is generated during configure,
	# however, for Debian builds a control file must be available before.
	# Therefore create a link to control.dummy.
	@ln -s control.dummy control

devclean: clean

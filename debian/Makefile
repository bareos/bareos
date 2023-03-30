#
# Makefile for preparations of Debian builds
#

all: copyright

copyright: copyright.header ../core/LICENSE copyright.core_exceptions ../webui/copyright copyright.license
	@cat copyright.header > copyright
	@sed -r -e 's/^$$/./' -e 's/^[ ]*(.*)$$/    \1/' ../core/LICENSE >> copyright
	@cat copyright.core_exceptions >> copyright
	@echo >> copyright
	@echo >> copyright
	@# add webui copyright, except the header stanza
	@sed -n '/^Files: /,$$p' ../webui/copyright >> copyright
	@cat copyright.license >> copyright

clean:
	@rm -f *~ *.log
	@rm -f -r tmp/*

distclean: clean
	@find . -type f -a -name "*.in" -a ! -name "control.in" | sed -e 's/.in$$//' | xargs rm -f

devclean: clean

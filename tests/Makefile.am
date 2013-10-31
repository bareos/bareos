ACLOCAL_AMFLAGS = -I m4
SUBDIRS=

if COVERAGE
clean: clean-am
	nodefiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$nodefiles" || $(RM) $$nodefiles
	datafiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$datafiles" || $(RM) $$datafiles
endif

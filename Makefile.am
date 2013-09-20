ACLOCAL_AMFLAGS = -I m4

SUBDIRS=libdroplet examples utests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = droplet-3.0.pc

EXTRA_DIST = droplet-3.0.pc.in

docs:
	$(RM) -r doc/html doc/man
	doxygen doc/Doxyfile

cscope:
	$(RM) cscope.out
	find $(SUBDIRS) -type f -name '*.[ch]' | cscope -b -k -i -

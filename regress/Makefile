#
# Makefile for Bareos regression testing
#
# Before running, you must create a file named config containing
#   the configuration parameters.  Create it by doing:
#
#  cp prototype.conf config
#
# Then edit config and set the value for what are appropriate for you.
#

nothing:

setup: prepare bareos sed

#
# Some machines cannot handle the sticky bit and other garbage that
#  is in weird-files, so we load and run it only on Linux machines.
#
bareos: prepare
	echo "Doing: scripts/setup"
	scripts/setup

prepare:
	@rm -rf weird-files
	# ignore errors
	tar -xzf weird-files.tar.gz || true
	@rm -rf tmp working dumps
	mkdir tmp working dumps

sed:
	echo "Doing: scripts/do_sed"
	scripts/do_sed

# Run all disk tests
test:
	./all-disk-tests

# run all tape and disk tests
full_test:
	./all-tape-and-disk-tests

# These tests require you to run as root
root_test:
	./all-root-tests

DEST = $(DESTDIR)/var/lib/bareos/bareos-regress
install:
	mkdir -p $(DEST)
	# directories
	rsync -a configs $(DEST)
	rsync -a data $(DEST)
	rsync -a scripts $(DEST)
	rsync -a tests $(DEST)
	# files
	rsync -a \
		CTestCustom.cmake \
		DartConfiguration.tcl* DartTestfile.txt* \
		Makefile all-* \
		debug do_* \
		encrypt-* endtime experimental-* \
		make-ctest-adds	misc-tests \
		nightly-* prototype.conf \
		rtest run* \
		startover_libdbi.sh starttime tape \
		weird-files.tar.gz \
		$(DEST)
	mkdir -p $(DEST)/bin
	mkdir -p $(DEST)/dumps
	mkdir -p $(DEST)/tmp
	mkdir -p $(DEST)/working

clean:
	scripts/cleanup
	rm -f tmp/file-list
	rm -fr tmp/* working/* dumps/* Testing
	rm -f test.out
	rm -f diff
	rm -f 1 2 3 scripts/1 scripts/2 scripts/3 tests/1 tests/2 tests/3
	find . -name .#* -exec rm -rf {} \;
	rm -Rvf build/cmake-build
	rm -f build

# Reset our userid after running as root
reset:
	chown -R ${USER}:${USER} . tmp working
	scripts/cleanup
	rm -f tmp/* working/*

distclean: clean
	rm -rf bin build weird-files weird-files weird-files2 tmp working
	rm -f scripts/*.conf

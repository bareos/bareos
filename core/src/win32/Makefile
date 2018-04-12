#
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
include Makefile.inc

VERSION_STRING := $(shell grep 'VERSION "' ../include/version.h | cut -d ' ' -f3)

all_subdirs = lib findlib lmdb filed console tools cats dird stored plugins lib/unittests tests
ifeq ($(BUILD_QTGUI),yes)
all_subdirs += qt-tray-monitor
endif

all: rc-files qt-gui
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

install:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

clean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

distclean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

ifeq ($(BUILD_QTGUI),)
rc-files: tools/bsmtpres.rc \
	  tools/bregexres.rc \
	  tools/bwildres.rc \
	  console/consoleres.rc \
	  filed/filedres.rc \
	  stored/storedres.rc \
	  stored/bextractres.rc \
	  stored/blsres.rc \
	  stored/bscanres.rc \
	  stored/btaperes.rc \
	  dird/dirdres.rc \
	  dird/dbcheckres.rc

qt-gui:
else
rc-files: tools/bsmtpres.rc \
	  tools/bregexres.rc \
	  tools/bwildres.rc \
	  console/consoleres.rc \
	  filed/filedres.rc \
	  stored/storedres.rc \
	  stored/bextractres.rc \
	  stored/blsres.rc \
	  stored/bscanres.rc \
	  stored/btaperes.rc \
	  dird/dirdres.rc \
	  dird/dbcheckres.rc \
	  qt-tray-monitor/traymon.rc

qt-gui: qt-tray-monitor/Makefile

qt-tray-monitor/Makefile: qt-tray-monitor/tray-monitor.pro
	cd qt-tray-monitor; $(QMAKE) $(QMAKE_ARGS) tray-monitor.pro

qt-tray-monitor/traymon.rc: qt-tray-monitor/traymon.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@
endif

tools/bsmtpres.rc: tools/bsmtpres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

tools/bregexres.rc: tools/bregexres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

tools/bwildres.rc: tools/bwildres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

console/consoleres.rc: console/consoleres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

filed/filedres.rc: filed/filedres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

stored/storedres.rc: stored/storedres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

dird/dirdres.rc: dird/dirdres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

dird/dbcheckres.rc: dird/dbcheckres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

stored/bextractres.rc: stored/bextractres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

stored/btaperes.rc: stored/btaperes.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

stored/blsres.rc: stored/blsres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

stored/bscanres.rc: stored/bscanres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

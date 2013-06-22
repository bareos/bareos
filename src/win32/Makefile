include Makefile.inc

VERSION_STRING := $(shell grep 'VERSION "' ../include/version.h | cut -d ' ' -f3)

all_subdirs = lib findlib filed console plugins
ifeq ($(BUILD_QTGUI),yes)
all_subdirs += qt-console qt-tray-monitor
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
rc-files: console/consoleres.rc filed/filedres.rc

qt-gui:
else
rc-files: console/consoleres.rc filed/filedres.rc qt-console/batres.rc qt-tray-monitor/traymon.rc

qt-gui: qt-console/Makefile qt-tray-monitor/Makefile

qt-console/Makefile: qt-console/bat.pro
	cd qt-console; $(QMAKE) bat.pro

qt-tray-monitor/Makefile: qt-tray-monitor/tray-monitor.pro
	cd qt-tray-monitor; $(QMAKE) tray-monitor.pro

qt-console/batres.rc: qt-console/batres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

qt-tray-monitor/traymon.rc: qt-tray-monitor/traymon.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@
endif

console/consoleres.rc: console/consoleres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

filed/filedres.rc: filed/filedres.rc.in
	@sed -e 's/@VERSION@/$(VERSION_STRING)/' $< > $@

ECHO_CMD=@

WIN32_DIRS=lib cats filed filed/plugins dird stored \
	 console wx-console tools scripts

WIN64_DIRS=lib filed filed/plugins console scripts

ifeq ($(WIN64),yes)
	 DIRS=$(WIN64_DIRS)
	 INSTALLER=win64_installer
	 SPECIAL=bsmtp
else
	 DIRS=$(WIN32_DIRS)
	 INSTALLER=win32_installer
	 SPECIAL=bat
endif

.PHONY: $(DIRS) clean all Makefile.inc win32_installer full_win32_installer \
	 win64_installer bsmtp

all: Makefile.inc $(DIRS) $(SPECIAL) $(INSTALLER)

clean: $(DIRS) win32_installer win64_installer
	$(ECHO_CMD)-rm -rf release32 release64

bat:
	(cd ../qt-console; ./make-win32)

full_win32_installer:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; exit 1; \
	   fi ; \
	fi

win32_installer:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; exit 1; \
	   fi ; \
	fi

win64_installer:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; exit 1; \
	   fi ; \
	fi


$(DIRS):
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; exit 1; \
	   fi ; \
	fi

bsmtp:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C tools bsmtp $(MAKECMDGOALS); then \
		echo "\n===== Make of tools/bsmtp succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of tools/bsmtp failed !!!!!\n\n" ; exit 1; \
	   fi ; \
	fi



Makefile.inc:
	@echo Creating $@
	$(ECHO_CMD)TOPDIR=`(cd ../../..;pwd)`; \
	TOPDIR=$${DEPKGS:-$${TOPDIR}}; \
	if test "$(WIN64)" = yes -a -e $${TOPDIR}/cross-tools/mingw-w64/bin/x86_64-pc-mingw32-gcc; then \
		BINDIR=$${TOPDIR}/cross-tools/mingw-w64/bin; \
		INCDIR=$${TOPDIR}/cross-tools/mingw-w64/x86_64-pc-mingw32/include; \
		DLLDIR=$${TOPDIR}/cross-tools/mingw-w64/x86_64-pc-mingw32/bin; \
		DEPKGSDIR=depkgs-mingw-w64; \
		DEPKGSDIR32=depkgs-mingw32; \
	elif test -e $${TOPDIR}/cross-tools/mingw32/bin/mingw32-gcc; then \
		BINDIR=$${TOPDIR}/cross-tools/mingw32/bin; \
		INCDIR=$${TOPDIR}/cross-tools/mingw32/mingw32/include; \
		DLLDIR=$${TOPDIR}/cross-tools/mingw32/mingw32/bin; \
		DEPKGSDIR=depkgs-mingw32; \
		DEPKGSDIR32=depkgs-mingw32; \
	else \
		echo "\nThe GCC cross compiler isn't installed."; \
		echo "You must run build-win32-cross-tools and build-dependencies first.\n"; \
		exit 1; \
	fi ; \
	BUILDDIR=`(pwd)`; \
	MAINDIR=`(cd ../..;pwd)`; \
	sed \
		-e "s^@WIN64@^$${WIN64}^" \
		-e "s^@WIN32DEPKGS@^$${DEPKGSDIR}^" \
		-e "s^@WIN32DEPKGS32@^$${DEPKGSDIR32}^" \
		-e "s^@WIN32BUILDDIR@^$${BUILDDIR}^" \
		-e "s^@WIN32MAINDIR@^$${MAINDIR}^" \
		-e "s^@WIN32TOPDIR@^$${TOPDIR}^" \
		-e "s^@WIN32BINDIR@^$${BINDIR}^" \
		-e "s^@WIN32INCDIR@^$${INCDIR}^" \
		-e "s^@WIN32DLLDIR@^$${DLLDIR}^" < Makefile.inc.in > $@

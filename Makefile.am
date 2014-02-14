install: install-am
	mkdir -p $(DESTDIR)$(pkgdatadir)
	mkdir -p $(DESTDIR)/@HTTPD_CONF@
	mkdir -p $(DESTDIR)@sysconfdir@/sudoers.d/
#	dnl #mkdir -p $(DESTDIR)@sysconfdir@/@PACKAGE@
#	dnl #mkdir -p $(DESTDIR)@sbindir@
	cp -pr config/ data/ module/ public/ $(DESTDIR)$(pkgdatadir)

#	#mkdir -p $(DESTDIR)$(pkgdatadir)/install
	$(INSTALL) -m 0640 install/apache/bareos-console-web.conf  $(DESTDIR)@HTTPD_CONF@/bareos-console-web.conf

	$(INSTALL) -m 0440 install/sudoers.d/bareos-console-web-bconsole $(DESTDIR)@sysconfdir@/sudoers.d/

#	#mv $(DESTDIR)$(pkgdatadir)/application/config.ini  $(DESTDIR)@sysconfdir@/@PACKAGE@/config.ini#
#	#ln -s @sysconfdir@/@PACKAGE@/config.ini  $(DESTDIR)$(pkgdatadir)/application/config.ini

#	#$(INSTALL_SCRIPT) install/webacula-config $(DESTDIR)@sbindir@

install: install-am
	mkdir -p $(DESTDIR)$(pkgdatadir)
	mkdir -p $(DESTDIR)/@HTTPD_CONF@
	mkdir -p $(DESTDIR)@sysconfdir@/sudoers.d/
#	dnl #mkdir -p $(DESTDIR)@sysconfdir@/@PACKAGE@
#	dnl #mkdir -p $(DESTDIR)@sbindir@
	cp -p init_autoloader.php $(DESTDIR)$(pkgdatadir)
	cp -pr config/ data/ module/ public/ $(DESTDIR)$(pkgdatadir)

#	#mkdir -p $(DESTDIR)$(pkgdatadir)/install
	$(INSTALL) -m 0640 install/apache/bareos-webui.conf  $(DESTDIR)@HTTPD_CONF@/

	$(INSTALL) -m 0440 install/sudoers.d/bareos-webui-bconsole $(DESTDIR)@sysconfdir@/sudoers.d/

#	#mv $(DESTDIR)$(pkgdatadir)/application/config.ini  $(DESTDIR)@sysconfdir@/@PACKAGE@/config.ini#
#	#ln -s @sysconfdir@/@PACKAGE@/config.ini  $(DESTDIR)$(pkgdatadir)/application/config.ini

#	#$(INSTALL_SCRIPT) install/bareos-webui-config $(DESTDIR)@sbindir@

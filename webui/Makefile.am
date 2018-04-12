SUBDIRS = module/Application/language public/js/locale

install: install-am
	mkdir -p $(DESTDIR)$(pkgdatadir)
	mkdir -p $(DESTDIR)/@HTTPD_CONF@
	mkdir -p $(DESTDIR)/etc/bareos/bareos-dir.d/console/
	mkdir -p $(DESTDIR)/etc/bareos/bareos-dir.d/profile/
	mkdir -p $(DESTDIR)/etc/bareos-webui/

	cp -pr version.txt init_autoloader.php config/ data/ module/ public/ vendor/ $(DESTDIR)$(pkgdatadir)
	cp -p install/directors.ini $(DESTDIR)/etc/bareos-webui/directors.ini
	cp -p install/configuration.ini $(DESTDIR)/etc/bareos-webui/configuration.ini
	$(INSTALL) -m 0640 install/bareos/bareos-dir.d/console/admin.conf.example $(DESTDIR)/etc/bareos/bareos-dir.d/console/
	$(INSTALL) -m 0640 install/bareos/bareos-dir.d/profile/webui-admin.conf $(DESTDIR)/etc/bareos/bareos-dir.d/profile/
	$(INSTALL) -m 0640 install/apache/bareos-webui.conf $(DESTDIR)@HTTPD_CONF@/


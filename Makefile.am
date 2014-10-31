install: install-am
	mkdir -p $(DESTDIR)$(pkgdatadir)
	mkdir -p $(DESTDIR)/@HTTPD_CONF@

	cp -pr init_autoloader.php config/ data/ module/ public/ vendor/ $(DESTDIR)$(pkgdatadir)
	cp -p  config/autoload/local.php.dist $(DESTDIR)/etc/bareos-webui.conf.php
	ln -s  /etc/bareos-webui.conf.php $(DESTDIR)/$(pkgdatadir)/config/autoload/local.php
	$(INSTALL) -m 0640 install/apache/bareos-webui.conf  $(DESTDIR)@HTTPD_CONF@/

#	#$(INSTALL_SCRIPT) install/bareos-webui-config $(DESTDIR)@sbindir@

Bareos WebUI
============

[![Build Status](https://travis-ci.org/bareos/bareos-webui.png?branch=master)](https://travis-ci.org/bareos/bareos-webui)

A PHP-Frontend to manage [Bareos](http://www.bareos.org/) over the web.

For a feature list and a few screenshots from alpha, please take a look 
at the [Project Homepage](http://bareos.github.io/bareos-webui/index.html).

### SYSTEM REQUIREMENTS

* A working Bareos Environment, Bareos 12.4 or later, including a Bareos catalog database (Currently only PostgreSQL is supported by the Web-Frontend, MySQL should be in near future.)
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenvif enabled
* PHP 5.3.3 or later; we recommend using the latest PHP version whenever possible
* PHP PDO Extension
* PHP intl Extension
* PHP PHAR Extension (for development purposes)
* PHP DATE Extension
* PHP OpenSSL Extension (for development purposes)
* Zend Framework 2.2.x
* A Browser with JavaScript enabled

### INSTALLATION

Please see [doc/install/INSTALL.md](doc/install/INSTALL.md).

### LICENSES

The Bareos WebUI is licensed under AGPL Version 3. 
You can find a copy of this license in [LICENSE.txt](LICENSE.txt).

* Zend Framework 2 - BSD 3-Clause License
* Twitter Bootstrap - Apache License v2.0
* jQuery - MIT License
* jqplot - dual licensed under MIT License and GPL Version 2 License

### BUGTRACKER

[http://bugs.bareos.org](http://bugs.bareos.orgi)


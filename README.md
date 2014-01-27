Barbossa
========

A PHP-Frontend to manage Bareos over the web.

### RELEASE INFORMATION

Currently, there is no official release available, but there are some screenshots from alpha in the [wiki](https://github.com/fbergkemper/barbossa/wiki/Screenshots).

### FEATURES

* PostgreSQL database support
* MySQL database support (not implemented yet)
* Detailed information on Jobs, Clients, Filesets, Pools, Volumes, Storage and Logs
* Multilingual support - standard gettext (.mo) files. Currently supported languages: English, ...
* Responsive Webdesign

### SYSTEM REQUIREMENTS

* A working Bareos Environment, Bareos 12.4 or later
* A Bareos Catalog database (Currently only PostgreSQL is supported.)
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenvif enabled
* PHP 5.3.3 or later; we recommend using the latest PHP version whenever possible
* PHP PDO Extension
* PHP intl Extension
* PHP PHAR Extension
* PHP DATE Extension
* PHP OpenSSL Extension
* Zend Framework 2.2.x or later
* A Browser of your choice with JavaScript enabled

### INSTALLATION

Please see [install/README.md](install/README.md).

### CONTRIBUTION

If you wish to contribute to barbossa, please read both the
[CONTRIBUTION.md](CONTRIBUTION.md) and [README-GIT.md](README-GIT.md) file.

### LICENSES

Barbossa is licensed under AGPL Version 3, unless there is no other license 
mentioned, e.g. in external libraries, frameworks etc., which are beeing used
by Barbossa.

* Zend Framework 2 - BSD 3-Clause License 
* ZendSkeletonApplication - MIT/BSD like License
* Composer Dependency Manager for PHP - MIT License
* jQuery - MIT License
* jqplot - dual licensed under MIT and GPL version 2 licenses
* Twitter Bootstrap - Apache License v2.0


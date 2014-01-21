barbossa
========

A PHP-Frontend to manage Bareos over the web.

### RELEASE INFORMATION

Currently, there is no official release available, but there are some screenshots from alpha in the [wiki](https://github.com/fbergkemper/barbossa/wiki/Screenshots).

### FEATURES

* PostgreSQL database support
* Detailed information on Jobs, Clients, Filesets, Pools, Volumes, Storage and Logs
* Multilingual support - standard gettext (.mo) files. Currently supported languages: English, ...

### SYSTEM REQUIREMENTS

* A working Bareos Environment, Bareos 12.4 or later
* A Bareos Catalog database (Currently only PostgreSQL is supported.)
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenvif enabled
* PHP 5.3.3 or later; we recommend using the latest PHP version whenever possible
* PHP PDO Extension
* PHP intl Extension
* PHP PHAR Extension
* PHP OpenSSL Extension
* Zend Framework 2.2.x or later
* A Browser of your choice with JavaScript enabled

### INSTALLATION

Please see [install/README.md](install/README.md).

### CONTRIBUTION

If you wish to contribute to barbossa, please read both the
[CONTRIBUTION.md](CONTRIBUTION.md) and [README-GIT.md](README-GIT.md) file.

### LICENSE


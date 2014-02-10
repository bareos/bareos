## INSTALL

As the project is in alpha state and there is no installation routine so far, 
this installation manual should help developers to get a working version on 
their system for development.

### SYSTEM REQUIREMENTS

* A working Bareos Environment, Bareos 12.4 or later, including a Bareos catalog database (Currently only PostgreSQL is supported, MySQL should be in near future.)
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenvif enabled
* PHP 5.3.3 or later; we recommend using the latest PHP version whenever possible
* PHP PDO Extension
* PHP intl Extension
* PHP PHAR Extension
* PHP DATE Extension
* PHP OpenSSL Extension
* Zend Framework 2.2.x or later
* A Browser of your choice with JavaScript enabled

### Get the current version from github

* Change into your webservers root directory e.g. 

```
cd /var/www/htdocs
```

* Get the current version from github 

```
git clone https://github.com/fbergkemper/barbossa.git
```

* Change into the barbossa directory

* Download composer.phar 

```
wget https://getcomposer.org/composer.phar
```

* Run the composer.phar, to download all the dependencies (Zend Framework 2, Zend Developers tools) into the vendor directory. 

```
./composer.phar install
```

See [https://getcomposer.org/](https://getcomposer.org/) for more information and documentation about the composer dependency manager for PHP.

### Configure Apache

See the example file [barbossa/install/apache/bareos.conf](https://raw.github.com/fbergkemper/barbossa/master/doc/install/apache/bareos.conf).
You might configure your Apache manually or copy the example configuration with wget from the git repo, e.g.

```
cd /etc/apache2/conf.d
wget https://raw2.github.com/fbergkemper/barbossa/master/install/apache/bareos.conf
```

### Configure the database connection

* Copy barbossa/config/autoload/local.php.dist to barbossa/config/autoload/local.php and edit the local.php file to your needs.

### Configuration to be able to run bconsole commands within the web-frontend

In order to be able to execute bconsole commands within the web-frontend, it is necessary there is a sudo rule for the user under
which your webserver is running. So, run visudo and add the following lines, e.g:

```
# bareos web-frontend entry
wwwrun ALL=NOPASSWD: /usr/sbin/bconsole
```

or something like ...

```
# bareos web-frontend entry
apache ALL=NOPASSWD: /usr/sbin/bconsole
```

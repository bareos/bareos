## INSTALL

Remember: this project is still in alpha state.

### SYSTEM REQUIREMENTS

* A working Bareos environment, Bareos 12.4 or later, including a Bareos catalog database (currently only PostgreSQL is tested)
* A working Bareos Console (bconsole) on this system
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenvif enabled
* PHP 5.3.3 or later
  * PHP PDO Extension
  * PHP intl Extension
  * PHP DATE Extension
* Zend Framework 2.2.x or later
* A Browser of your choice with JavaScript enabled
* when using composer.phar (installer for additional components):
  * PHP PHAR Extension
  * PHP OpenSSL Extension

### How bareos-webui accesses Bareos

Bareos-webui connects to Bareos by
  * bconsole
  * database connection (read-only) to the Bareos catalog

## Installation

### Package based installation

The bareos-webui packages are available for a number of Linux distributions, see http://download.bareos.org/bareos/contrib/

#### Zend Framework 2

Zend Framework (>= 2.2) is required for bareos-webui.

However, not all distributions offer these packages.

* RedHat
  * https://apps.fedoraproject.org/packages/php-ZendFramework2
    * Fedora 20 (included)
    * Fedora 19 (included)
    * RHEL6/Centos6: https://fedoraproject.org/wiki/EPEL
* SUSE
  * available as part of the [Bareos contrib](http://download.bareos.org/bareos/contrib/) repository (based on EPEL version)
* Debian/Ubuntu
  * available as part of the [Bareos contrib](http://download.bareos.org/bareos/contrib/) repository


#### Installation

  * add the [Bareos contrib](http://download.bareos.org/bareos/contrib/) repository that is matching your Linux distribution
  * install the package bareos-webui
  * verify, that your local bconsole can connect to your Bareos environment, e.g. run ```bconsole```
  * configure your database connection to your Bareos catalog in ```/etc/bareos-webui.conf.php``` (this is the link target of ```/usr/share/bareos-webui/config/autoload/local.php```). See [configure database connection](#configure-the-database-connection)
  * reload your Apache webserver
  * test bareos-webui using the url: [http://localhost/bareos-webui](http://localhost/bareos-webui)
    * Attention: the default installation only allows access from localhost. This can be adapted in the ```bareos-webui.conf``` file, that is location in the Apache configuration files directory. However, when allowing access from other systems, you should also configure other access restrictions like user login.


### Installation from github sources

* Get we latest version from github, e.g.

```
cd /usr/share/
git clone https://github.com/bareos/bareos-webui.git
cd bareos-webui
```

* Download composer.phar 

```
wget https://getcomposer.org/composer.phar
```

* Run the composer.phar, to download all the dependencies (Zend Framework 2, Zend Developers tools) into the vendor directory:

```
./composer.phar install
```

See [https://getcomposer.org/](https://getcomposer.org/) for more information and documentation about the composer dependency manager for PHP.

### Configure Apache

See the example file [bareos-console-web.conf](https://raw.github.com/bareos/bareos-webui/master/install/apache/bareos-webui.conf).
You might configure your Apache manually or copy the example configuration with wget from the git repo, e.g.

```
cd /etc/apache2/conf.d
wget https://raw.github.com/bareos/bareos-webui/master/install/apache/bareos-webui.conf
```

Note: On Debian the ZF2_PATH Variable in your bareos-webui.conf is currently needed, if you are using the ZF2 package from Bareos contrib. 
This will change and no longer be necessary in future, but it is the way to go for the moment.

```
# Environment Variable for Zend Framwework 2 (Debian)
SetEnv "ZF2_PATH" "/usr/share/php5"
```

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

You can use the default sudo configuration from bareos-webui, that should work on most Linux distributions:
```
cd /etc/sudoers.d/
wget https://raw.github.com/bareos/bareos-webui/master/install/sudoers.d/bareos-webui-bconsole
chmod u=r,g=r,o= bareos-webui-bconsole
```

### Configure the database connection

Bareos-webui needs only a read-only connection to the Bareos catalog database, so there are multiple possibilities to configure bareos-webui:
  * reuse the exsiting Bareos database user
  * create a new (read-only) database user for bareos-webui

The first approach is simpler to configure, the second approach is more secure.

#### PostgreSQL

Bareos >= 14.1

In this example we use bareoswebui as database username. Attention: PostgreSQL user names are not allowd to contain -.
As database password <DATEBASE_PASSWORD>, choose a random password.

```
su - postgres
DB_USER=bareos_webui
DB_PASS=<DATEBASE_PASSWORD>
/usr/lib/bareos/scripts/bareos-config get_database_grant_priviliges postgresql $DB_USER $DB_PASS readonly > /tmp/database_grant_priviliges.sql
psql -d bareos -f /tmp/database_grant_priviliges.sql
rm /tmp/database_grant_priviliges.sql
vi data/pg_hba.conf
```

Add following lines before the "all" rules into ```pg_hba.conf```:
```
host    bareos    bareos_webui    127.0.0.1/32    md5
host    bareos    bareos_webui    ::1/128         md5
```

Reload the PostgreSQL configuration:
```
pg_ctl reload
```


##### 

Adapt the bareos-webui configuration file to match your database settings.

  * Manual installation:
    * Copy ```bareos-webui/config/autoload/local.php.dist``` to ```bareos-webui/config/autoload/local.php``` and edit the local.php file to your needs.
  * Package installation:
    * The file local.php is available as ```/etc/bareos-webui.conf.php```. It is the link target of the configuration file used by bareos-webui ```/usr/share/bareos-webui/config/autoload/local.php```. Please edit this file.


As the result, the configuration file should look similar to this:

```
return array(
        'db' => array(
                // Set your database driver here: Pdo_Mysql, Pdo_Pgsql, Mysqli or Pgsql
                'driver' => 'Pdo_Pgsql',
                // Set your database here
                'dbname' => 'bareos',
                // Set your hostname here
                'host' => 'localhost',
                // Set your username here
                'username' => 'bareos_webui',
                // Set your password here
                'password' => '<DATEBASE_PASSWORD>',
        ),
);
```

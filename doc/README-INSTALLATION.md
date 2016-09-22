INSTALLATION
============

## INSTALLATION FROM PACKAGE

Please see: http://doc.bareos.org/master/html/bareos-manual-main-reference.html#InstallingBareosWebui

## INSTALLATION FROM SOURCE

### System requirements

* A working Bareos environment, Bareos >= 16.2. The Bareos Director and Bareos Webui should have the same version.
* You can install Bareos Webui on any host it does not have to be installed on the same as the Bareos Director.
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenv enabled.
* PHP >= 5.3.23

### Get the source

You can download the source as a zip or tar.gz file from https://github.com/bareos/bareos-webui/releases .
Upload it to your webserver and extract it e.g. in you webservers root directory.

```
cd /srv/www/htdocs

wget https://github.com/bareos/bareos-webui/archive/Release/15.2.1.tar.gz

tar xf 15.2.1.tar.gz
```

### Configure your Apache Webserver

You can for example copy the default configuration file and edit it to your needs.

```
cd /etc/apache2/conf.d

cp /srv/www/htdocs/bareos-webui/install/apache/bareos-webui.conf .

vim bareos-webui.conf
```

The example configuration looks like following:

```
#
# Bareos WebUI Apache configuration file
#

# Environment Variable for Application Debugging
# Set to "development" to turn on debugging mode or
# "production" to turn off debugging mode.
<IfModule env_module>
        SetEnv "APPLICATION_ENV" "production"
</IfModule>

Alias /bareos-webui  /srv/www/htdocs/bareos-webui/public

<Directory /srv/www/htdocs/bareos-webui/public>

        Options FollowSymLinks
        AllowOverride None

        # Following module checks are only done to support
        # Apache 2.2,
        # Apache 2.4 with mod_access_compat and
        # Apache 2.4 without mod_access_compat
        # in the same configuration file.
        # Feel free to adapt it to your needs.

        # Apache 2.2
        <IfModule !mod_authz_core.c>
                Order deny,allow
                Allow from all
        </IfModule>

        # Apache 2.4
        <IfModule mod_authz_core.c>
                <IfModule mod_access_compat.c>
                    Order deny,allow
                </IfModule>
                Require all granted
        </IfModule>

        <IfModule mod_rewrite.c>
                RewriteEngine on
                RewriteBase /bareos-webui
                RewriteCond %{REQUEST_FILENAME} -s [OR]
                RewriteCond %{REQUEST_FILENAME} -l [OR]
                RewriteCond %{REQUEST_FILENAME} -d
                RewriteRule ^.*$ - [NC,L]
                RewriteRule ^.*$ index.php [NC,L]
        </IfModule>

        <IfModule mod_php5.c>
                php_flag magic_quotes_gpc off
                php_flag register_globals off
        </IfModule>

</Directory>
```

Restart your Apache Webserver.

### Configure the directors

Create the required configuration file /etc/bareos-webui/directors.ini and edit it to your needs.

***Note:*** The location and name of the directors.ini is mandatory.

```
mkdir /etc/bareos-webui

cd /etc/bareos-webui

cp /srv/www/htdocs/bareos-webui/install/directors.ini .

vim directors.ini
```

A basic directors.ini could look similar to this.

```
[director121-dir]
enabled = "yes"
diraddress = "10.10.121.100"
dirport = 9101

[director122-dir]
enabled = "yes"
diraddress = "10.10.122.100"
dirport = 9101

[director123-dir-cat-1]
enabled = "no"
diraddress = "10.10.123.100"
dirport = 9101
catalog= "MyCatalog"

[director123-dir-cat-2]
enabled = "no"
diraddress = "10.10.123.100"
dirport = 9101
catalog= "AnotherCatalog"
```

### Configure the webui

```
cd /etc/bareos-webui

cp /srv/www/htdocs/bareos-webui/install/configuration.ini .

vim configuration.ini
```

The configuration.ini currently looks like following. You are able to set the session timeout,
pagination, table behaviour and pooltype for labeling to your needs.

```
[session]
# Default: 3600 seconds
timeout=3600

[tables]
# Define a list of pagination values.
# Default: 10,25,50,100
pagination_values=10,25,50,100

# Default number of rows per page
# for possible values see pagination_values
# Default: 25
pagination_default_value=25

# State saving - restore table state on page reload.
# Default: false
save_previous_state=false

[autochanger]
# Pooltype for label to use as filter.
# See pooltype in output of bconsole: list pools
# Default: none
labelpooltype=scratch
```

### Create restricted named consoles

Configure restricted named consoles as well as profiles and reload your director configuration.
Those consoles are like user accounts and used by the webui.

For more details about console and profile resource configuration in bareos, please have a look at
http://doc.bareos.org/ .

```
Console {
  Name = user1
  Password = "user1"
  Profile = profile1
}
```

```
Profile {
  Name = webui-admin
  CommandACL = !.bvfs_clear_cache, !.exit, !.sql, !configure, !create, !delete, !purge, !sqlquery, !umount, !unmount, *all*
  Job ACL = *all*
  Schedule ACL = *all*
  Catalog ACL = *all*
  Pool ACL = *all*
  Storage ACL = *all*
  Client ACL = *all*
  FileSet ACL = *all*
  Where ACL = *all*
  Plugin Options ACL = *all*
}
```

After reloading the director you are done and able to use the webui.

Login via your favorite browser by calling: http://<host>/bareos-webui/.

### ADDITIONAL INFORMATION

#### SELinux

To install bareos-webui on a system with SELinux enabled, the following additional step have to be performed.

 * Allow HTTPD scripts and modules to connect to the network

```
setsebool -P httpd_can_network_connect on
```

#### NGINX

If you want to use bareos-webui on e.g. nginx with php5-fpm a basic working configuration could look like this.

```
server {

        listen       9100;
        server_name  bareos;
        root         /var/www/bareos-webui/public;

        location / {
                index index.php;
                try_files $uri $uri/ /index.php?$query_string;
        }

        location ~ .php$ {

                include snippets/fastcgi-php.conf;

                # With php5-cgi alone pass the PHP
                # scripts to FastCGI server
                # listening on 127.0.0.1:9000

                # fastcgi_pass 127.0.0.1:9000;

                # With php5-fpm:

                fastcgi_pass unix:/var/run/php5-fpm.sock;

                # Set APPLICATION_ENV to either 'production' or 'development'

                fastcgi_param APPLICATION_ENV production;
                # fastcgi_param APPLICATION_ENV development;

        }

}
```

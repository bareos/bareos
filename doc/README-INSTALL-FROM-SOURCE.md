
INSTALLATION
============

### SYSTEM REQUIREMENTS

* A working Bareos environment, Bareos >= 15.2
* An Apache 2.x Webserver with mod-rewrite, mod-php5 and mod-setenv
* PHP >= 5.3.3
    * PHP OpenSSL Extension
* Zend Framework 2.2.x or later

  **Note:** Unfortunately, not all distributions offer a Zend Framework 2 package.
  The following list shows where to get the Zend Framework 2 package.

  * RHEL, CentOS
    * https://fedoraproject.org/wiki/EPEL
    * https://apps.fedoraproject.org/packages/php-ZendFramework2

  * Fedora
    * https://apps.fedoraproject.org/packages/php-ZendFramework2

  * SUSE, Debian, Ubuntu
    * http://download.bareos.org/bareos

### PACKAGE BASED INSTALLATION

Packages are available for a number of Linux distributions, please see:

* [Version 14.2 (stable)](http://download.bareos.org/bareos/contrib/)
* [Version 15.2 (stable)](http://download.bareos.org/bareos/release/15.2/)
* [Version 15.3 (experimental/nightly)](http://download.bareos.org/bareos/experimental/nightly/)

#### Step 1 - Adding the Repository and install the package

If not already done, add the [Bareos](http://download.bareos.org/bareos/) repository that is matching your Linux distribution.
Please have a look at the [Bareos documentation](http://doc.bareos.org/master/html/bareos-manual-main-reference.html#InstallTheBareosSoftwarePackages) for more information on how to achieve this.

After adding the repository simply install the bareos-webui package via your package manager.

* RHEL, CentOS and Fedora

```
yum install bareos-webui
```

or

```
dnf install bareos-webui
```

* SUSE Linux Enterprise Server (SLES), openSUSE

```
zypper install bareos-webui
```

* Debian, Ubuntu

```
apt-get install bareos-webui
```

#### Step 2 - Configuration of restricted consoles and profile resources

You can have multiple Consoles with different names and passwords, sort of like multiple users, each with different privileges.
As a default, these consoles can do absolutely nothing – no commands whatsoever. You give them privileges or rather access to
commands and resources by specifying access control lists in the Director’s Console resource. The ACLs are specified by a directive
followed by a list of access names.

It is required to add at least one restricted named console in your director configuration (bareos-dir.conf) for the webui.
The restricted named consoles, configured in your bareos-dir.conf, are used for authentication and access control. The name
and password directives of the restricted consoles are the credentials you have to provide during authentication to the webui
as username and password. For full access and functionality relating the director connection the following commands are
currently needed by the webui and have to be made available via the CommandACL in your profile the restricted consoles uses.

* status
* messages
* show
* version
* run
* rerun
* cancel
* use
* restore
* list, llist
* .api
* .bvfs_update
* .bvfs_lsdirs
* .bvfs_lsfiles
* .bvfs_versions
* .bvfs_restore
* .jobs
* .clients
* .filesets

The package install provides a default console and profile configuration under /etc/bareos/bareos-dir.d/, which have to be included
at the bottum of your /etc/bareos/bareos-dir.conf and edited to your needs.

```
echo "@/etc/bareos/bareos-dir.d/webui-consoles.conf" >> /etc/bareos/bareos-dir.conf
echo "@/etc/bareos/bareos-dir.d/webui-profiles.conf" >> /etc/bareos/bareos-dir.conf
```

```
#
# Preparations:
#
# include this configuration file in bareos-dir.conf by
# @/etc/bareos/bareos-dir.d/webui-consoles.conf
#

#
# Restricted console used by bareos-webui
#
Console {
  Name = user1
  Password = "CHANGEME"
  Profile = webui
}

```
For more details about console resource configuration in bareos, please have a look at the online [Bareos documentation](http://doc.bareos.org/master/html/bareos-manual-main-reference.html#ConsoleResource).

```
#
# Preparations:
#
# include this configuration file in bareos-dir.conf by
# @/etc/bareos/bareos-dir.d/webui-profiles.conf
#

#
# bareos-webui default profile resource
#
Profile {
  Name = webui
  CommandACL = status, messages, show, version, run, rerun, cancel, .api, .bvfs_*, list, llist, use, restore, .jobs, .filesets, .clients
  Job ACL = *all*
  Schedule ACL = *all*
  Catalog ACL = *all*
  Pool ACL = *all*
  Storage ACL = *all*
  Client ACL = *all*
  FileSet ACL = *all*
  Where ACL = *all*
}
```
For more details about profile resource configuration in bareos, please have a look at the online [Bareos documentation](http://doc.bareos.org/master/html/bareos-manual-main-reference.html#ProfileResource).

**Note:** Do not forget to reload your new director configuration.

#### Step 3 - Configure your Apache Webserver

If you have installed from package, a default configuration is provided, please see /etc/apache2/conf.d/bareos-webui.conf,
/etc/httpd/conf.d/bareos-webui.conf or /etc/apache2/available-conf/bareos-webui.conf.

Required apache modules, setenv, rewrite and php are enabled via package postinstall script.
You simply need to restart your apache webserver manually.

#### Step 4 - Configure your /etc/bareos-webui/directors.ini

Configure your directors in */etc/bareos-webui/directors.ini* to match your settings,
which you have choosen in the previous steps.

The configuration file /etc/bareos-webui/directors.ini should look similar to this:

```
;
; Bareos WebUI Configuration
; File: /etc/bareos-webui/directors.ini
;

;
; Section localhost-dir
;
[localhost-dir]

; Enable or disable section. Possible values are "yes" or "no", the default is "yes".
enabled = "yes"

; Fill in the IP-Address or FQDN of you director.
diraddress = "localhost"

; Default value is 9101
dirport = 9101

; Note: TLS has not been tested and documented, yet.
;tls_verify_peer = false
;server_can_do_tls = false
;server_requires_tls = false
;client_can_do_tls = false
;client_requires_tls = false
;ca_file = ""
;cert_file = ""
;cert_file_passphrase = ""
;allowed_cns = ""

;
; Section remote-dir
;
[remote-dir]
enabled = "no"
diraddress = "hostname"
dirport = 9101
; Note: TLS has not been tested and documented, yet.
;tls_verify_peer = false
;server_can_do_tls = false
;server_requires_tls = false
;client_can_do_tls = false
;client_requires_tls = false
;ca_file = ""
;cert_file = ""
;cert_file_passphrase = ""
;allowed_cns = ""
```

**Note:** You can add as many directors as you want.

Now, you are able to login by calling http://<yourhost>/bareos-webui in your browser of choice.

### INSTALLATION FROM SOURCE

In this example we assume a running Apache2 Webserver with PHP5 and mod_rewrite is already installed and configured properly.

#### Step 1 - Get the source

```
cd /srv/www/htdocs

git clone https://github.com/bareos/bareos-webui.git
```

or

```
cd /srv/www/htdocs

wget https://github.com/bareos/bareos-webui/archive/Release/15.2.1.tar.gz

tar xf 15.2.1.tar.gz
```

#### Step 2 - Get Zend Framework 2

If not installed in other ways we get Zend Framework 2 via Composer.
Read more about Composer here: https://getcomposer.org/

```
cd bareos-webui

./composer.phar install
```

#### Step 3 - Configure Apache2 Webserver

You can for example copy the default configuration file and edit it to your needs.

```
cd /etc/apache2/conf.d

cp /srv/www/htdocs/bareos-webui/install/apache/bareos-webui.conf .

vim bareos-webui.conf
```

In this example it looks like following.

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

#### Step 4 - Configure the directors

Create a required configuration file /etc/bareos-webui/directors.ini and edit it to your needs.

***Note:*** The location and name of the directors.ini is mandatory.

```
mkdir /etc/bareos-webui

cd /etc/bareos-webui

cp /srv/www/htdocs/bareos-webui/install/directors.ini .

vim directors.ini
```

A basic directors.ini should look similar to this.

```
[director121-dir]
enabled = "yes"
diraddress = "10.10.121.100"
dirport = 9101

[director122-dir]
enabled = "yes"
diraddress = "10.10.122.100"
dirport = 9101

[director123-dir]
enabled = "no"
diraddress = "10.10.123.100"
dirport = 9101
```

#### Step 5 - Create restricted named consoles

Configure restricted named consoles and reload your director configuration. Those consoles are like user accounts and used by the webui.
Therefore you need need to create for example two files with the following content on the host your director runs on and you want to be
able to connect to via the webui.

```
#
# Preparations:
#
# include this configuration file in bareos-dir.conf by
# @/etc/bareos/bareos-dir.d/webui-consoles.conf
#

#
# Restricted consoles used by bareos-webui
#
Console {
  Name = user1
  Password = "user1"
  Profile = profile1
}
Console {
  Name = user2
  Password = "user3"
  Profile = profile2
}
Console {
  Name = user3
  Password = "user3"
  Profile = profile3
}
```

```
#
# Preparations:
#
# include this configuration file in bareos-dir.conf by
# @/etc/bareos/bareos-dir.d/webui-profiles.conf
#

#
# bareos-webui profile resources
#
Profile {
  Name = profile1
  CommandACL = status, messages, show, version, run, rerun, cancel, .api, .bvfs_*, list, llist, use, restore, .jobs, .filesets, .clients
  Job ACL = *all*
  Schedule ACL = *all*
  Catalog ACL = *all*
  Pool ACL = *all*
  Storage ACL = *all*
  Client ACL = *all*
  FileSet ACL = *all*
  Where ACL = *all*
}

Profile {
  Name = profile2
  CommandACL = status, messages, show, version, run, rerun, cancel, .api, .bvfs_*, list, llist, use, restore, .jobs, .filesets, .clients
  Job ACL = *all*
  Schedule ACL = *all*
  Catalog ACL = *all*
  Pool ACL = *all*
  Storage ACL = *all*
  Client ACL = *all*
  FileSet ACL = *all*
  Where ACL = *all*
}

Profile {
  Name = profile3
  CommandACL = status, messages, show, version, run, rerun, cancel, .api, .bvfs_*, list, llist, use, restore, .jobs, .filesets, .clients
  Job ACL = *all*
  Schedule ACL = *all*
  Catalog ACL = *all*
  Pool ACL = *all*
  Storage ACL = *all*
  Client ACL = *all*
  FileSet ACL = *all*
  Where ACL = *all*
}
```

After including both files and reloading the director you are done and able to use the webui.

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

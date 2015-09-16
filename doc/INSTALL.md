
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

#### Step 6 - SELinux

If you do not use SELinux on your system, you can skip this step.

To install bareos-webui on a system with SELinux enabled, the following additional step must be performed.

 * Allow HTTPD scripts and modules to connect to the network

```
setsebool -P httpd_can_network_connect on
```


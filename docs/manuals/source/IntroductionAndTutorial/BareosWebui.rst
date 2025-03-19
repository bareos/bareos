
.. _section-webui:

Bareos Webui
============

.. index::
   single: Webui
   single: Webui; Install

Since :sinceVersion:`15.2.0: bareos-webui` |webui| is part of the Bareos project and available for a number of platforms.

.. image:: /include/images/bareos-webui-jobs.*
   :width: 80.0%

Features
--------

-  Intuitive web interface

-  Multilingual

-  Can access multiple directors and catalogs

-  Individual accounts and ACL support via Bareos restricted named consoles

-  Tape Autochanger management with the possibility to label, import/export media and update your autochanger slot status

-  Temporarly enable or disable jobs, clients and schedules and also see their current state

-  Show

   -  Detailed information about Jobs, Clients, Filesets, Pools, Volumes, Storages, Schedules, Logs and Director messages

   -  Filedaemon, Storage- and Director updates

   -  Client, Director, Storage and Scheduler status

-  Backup Jobs

   -  Start, cancel, rerun and restore from

   -  Show the file list of backup jobs

-  Restore files by browsing through a filetree of your backup jobs

   -  Merge your backup jobs history and filesets of a client or use a single backup job for restore

   -  Restore files to a different client instead of the origin

-  bconsole interface (limited to non-interactive commands)

System Requirements
-------------------

-  A platform for which the **bareos-webui** package is available, see :ref:`section-BareosPackages`

-  A working Bareos environment

-  |dir| version and |webui| version must match (major need to be identical)

-  The |webui| can be installed on any host. It does not have to be installed on the same as the |dir|.

-  The default installation uses PHP-FPM with Apache HTTP webserver having mod-rewrite and mod-fcgid enabled.

-  PHP 7 or newer is recommended.

-  On SUSE Linux Enterprise 12 you need the additional SUSE Linux Enterprise Module for Web Scripting 12 and additional repository :strong:`Package Hub` to satisfy apache2-mod-fcgid requirement.

.. _section-install-webui:

Installation
------------

Adding the Bareos Repository
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If not already done, add the Bareos repository that is matching your Linux distribution. Please have a look at the chapter :ref:`section-InstallBareosPackages` for more information on how to achieve this.

Install the bareos-webui package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After adding the repository simply install the bareos-webui package via your package manager.

-  RHEL, CentOS and Fedora

   .. code-block:: shell-session

      yum install bareos-webui

   or

   .. code-block:: shell-session

      dnf install bareos-webui

-  SUSE Linux Enterprise Server (SLES), openSUSE

   .. code-block:: shell-session

      zypper install bareos-webui

-  Debian, Ubuntu

   .. code-block:: shell-session

      apt-get install bareos-webui

Because php-fpm support is not automatically added to Apache2 on Debian like platforms, you have to issue those commands to enable it.
Replace :command:`php8.1-fpm` by the version you have installed.

.. code-block:: shell-session
   :caption: Debian, Ubuntu enabling php8-fpm support on Apache2 example

   a2enmod proxy_fcgi setenvif
   a2enconf php8.1-fpm
   systemctl reload apache2



Minimal Configuration
~~~~~~~~~~~~~~~~~~~~~

This assumes, |dir| and |webui| are installed on the same host.

#. If you are using SELinux, see :ref:`section-webui-selinux`.

#. If you are using AppArmor, see :ref:`section-webui-apparmor`.

#. Restart Apache, see :ref:`section-webui-apache`.

#. Use :command:`bconsole` to create a user with name **admin** and password **secret** and permissions defined in :config:option:`Dir/Profile = webui-admin`\ :

   .. code-block:: bconsole

      *<input>reload</input>
      reloaded
      *<input>configure add console name=admin password=secret profile=webui-admin tlsenable=false</input>

   Of course, you can choose other names and passwords. For details, see :ref:`section-webui-console`.

   Or rename :file:`/etc/bareos/bareos-dir.d/console/admin.conf.example` to `.conf` and reload |dir| configuration.

#. Login to http://HOSTNAME/bareos-webui with username and password as created.


.. _section-upgrade-webui:

Upgrading |webui|
-----------------

Upgrade from Bareos 21 or lower to Bareos 22
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

|webui| now requires **php-fpm** instead of Apache **mod-php**.

Usually this should be automatically handled by the packagemanager while updating.

Please consider the following remarks below according to your operating system of choice
if auto upgrading does not work for you.

If the |webui| is not reachable after upgrading, make sure that:

- Apache **mod-php** is disabled or has been removed

- **php-fpm*** usage is configured properly in Apache

- **php-fpm** service is enabled and has been restarted

- Apache service has been restarted

- SELinux or AppArmor configuration are done

Debian, Ubuntu, Univention
^^^^^^^^^^^^^^^^^^^^^^^^^^

Please use `apt` instead of `apt-get` to upgrade automatically.


SUSE Linux Enterprise Server (SLES), openSUSE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Before upgrading
________________

- Disable or remove any PHP module in Apache2

After upgrading
_______________

- Ensure a php.ini file is in place (e.g. `/etc/php7/fpm/php.ini` or `/etc/php8/fpm/php.ini`)

- Configure PHP-FPM to your needs (e.g. `/etc/php7/fpm/php-fpm.conf` and `/etc/php7/fpm/php-fpm.d/www.conf`)

- Configure mod_fcgid to your needs `/etc/apache2/conf.d/mod_fcgid.conf`

A minimal example may look like following.

.. code-block:: bareosconfig
   :caption: /etc/apache2/conf.d/mod_fcgid.conf

   <IfModule fcgid_module>
        DirectoryIndex index.php
        <FilesMatch "\.php$">
                SetHandler "proxy:fcgi://127.0.0.1:9000/"
                #CGIPassAuth on
        </FilesMatch>
   </IfModule>

- Restart Apache2 and PHP-FPM

.. code-block:: shell-session

   systemctl restart apache2 php-fpm

Console/Profile changes
^^^^^^^^^^^^^^^^^^^^^^^

The |webui| restore form now accepts a `plugin options string` in the restore form for plugin restores.
Therefore, the |webui| Director profile :config:option:`dir/profile = webui-admin` has been extended by the directive :config:option:`dir/profile/PluginOptionsACL = *all*` in Bareos :sinceVersion:`22.0.0: PluginOptionsACL = *all* added to profile webui-admin`.
To make use of that feature in |webui|, a proper configured :config:option:`dir/profile/PluginOptionsACL` is required.


Additional detailed information
-------------------------------

.. _section-webui-console:

Create a restricted consoles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There is no need, that |webui| itself provide a user management. Instead it uses so named :config:option:`Dir/Console`\  defined in the |dir|. You can have multiple consoles with different names and passwords, sort of like multiple users, each with different privileges.

At least one :config:option:`Dir/Console`\  is required to use the |webui|.

To allow a user with name **admin** and password **secret** to access the |dir| with permissions defined in the :config:option:`Dir/Profile = webui-admin`\  (see :ref:`section-webui-profile`), either

-  create a file :file:`/etc/bareos/bareos-dir.d/console/admin.conf` with following content:

   .. code-block:: bareosconfig
      :caption: bareos-dir.d/console/admin.conf

      Console {
        Name = "admin"
        Password = "secret"
        Profile = "webui-admin"
        TlsEnable = false
      }

   To enable this, reload or restart your |dir|.

-  or use the :command:`bconsole`:

   .. code-block:: bconsole

      *<input>configure add console name=admin password=secret profile=webui-admin tlsenable=false</input>

   If the profile could not be found, reload or restart your |dir|.

   TLS-PSK is not available between the |webui| and the |dir|. To enable TLS with certificates, see :ref:`TransportEncryptionWebuiBareosDirChapter`.

For details, please read :ref:`DirectorResourceConsole`.

.. _section-webui-profile:

Configuration of profile resources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The package **bareos-webui** comes with a predefined profile for |webui|: :config:option:`Dir/Profile = webui-admin`\ .

If your |webui| is installed on another system than the |dir|, you have to copy the profile to the |dir|.

This is the default profile, giving access to all Bareos resources and allowing all commands used by the |webui|:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/profile/webui-admin.conf

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

The :config:option:`Dir/Profile`\  itself does not give any access to the |dir|, but can be used by :config:option:`Dir/Console`\ , which do give access to the |dir|, see :ref:`section-webui-console`.

For details, please read :ref:`section-webui-access-control-configuration` and :ref:`DirectorResourceProfile`.

.. _section-webui-selinux:

SELinux
~~~~~~~

.. index::
   single: SELinux; bareos-webui

To use the |webui| on a system with SELinux enabled, permission must be given to HTTPD to make network connections:

.. code-block:: shell-session
   :caption: example of AppArmor php-fpm profile /etc/apparmor.d/local/php-fpm

   setsebool -P httpd_can_network_connect on


.. _section-webui-apparmor:

AppArmor
~~~~~~~~

.. index::
   single: AppArmor; bareos-webui

To use the |webui| on a system with AppArmor enabled, some permissions must be given to php-fpm.

The following example can be saved to :file:`/etc/apparmor.d/local/php-fpm`\.
You have to adapt the path of your php-fpm configured session.save_path.

.. code-block:: shell-session
   :caption: example of AppArmor php-fpm profile /etc/apparmor.d/local/php-fpm

   cat > /etc/apparmor.d/local/php-fpm <<<"# Site-specific additions and overrides for 'php-fpm'
   # bareos-webui
   /usr/share/bareos-webui/** r,
   /etc/bareos-webui/directors.ini r,
   /etc/bareos-webui/configuration.ini r,
   /var/lib/php8/sessions/** w,
   "
   systemctl reload apparmor.service

You can use tools like :command:`aa-logprof` to check if any other AppArmor restriction are raised during webui usage.
You can then add the needed resource to the profile.


.. _section-webui-transportencryption:

TransportEncryption
~~~~~~~~~~~~~~~~~~~

.. index::
   single: Transport Encryption; bareos-webui

See the dedicated chapter and section :ref:`TransportEncryptionWebuiBareosDirChapter`\.



.. _section-webui-apache:

Configure your Apache Webserver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Apache; bareos-webui

The package **bareos-webui** provides a default configuration for Apache. Depending on your distribution, it is installed at :file:`/etc/apache2/conf.d/bareos-webui.conf`, :file:`/etc/httpd/conf.d/bareos-webui.conf`, :file:`/etc/apache2/available-conf/bareos-webui.conf` or similar.

The required Apache modules, :strong:`fcgid` and :strong:`rewrite` are enabled via package postinstall script. However, after installing the **bareos-webui** package, you need to restart your Apache webserver manually.


.. _section-webui-configuration-files:

Configure your /etc/bareos-webui/directors.ini
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Configuration; WebUI

Configure your directors in :file:`/etc/bareos-webui/directors.ini` to match your settings.

The configuration file :file:`/etc/bareos-webui/directors.ini` should look similar to this:

.. code-block:: bareosconfig
   :caption: /etc/bareos-webui/directors.ini

   ;
   ; Bareos WebUI Configuration File
   ;
   ; File: /etc/bareos-webui/directors.ini
   ;

   ;------------------------------------------------------------------------------
   ; Section localhost-dir
   ;------------------------------------------------------------------------------
   [localhost-dir]

   ; Enable or disable section. Possible values are "yes" or "no", the default is "yes".
   enabled = "yes"

   ; Fill in the IP-Address or FQDN of you director.
   ; E.g. alice.example.com, 127.0.0.1 or [::1]
   address = "localhost"

   ; Default value is 9101
   port = 9101

   ; Set catalog to explicit value if you have multiple catalogs
   ;catalog = "MyCatalog"

   ; TLS verify peer
   ; Possible values: true or false
   tls_verify_peer = false

   ; Server can do TLS
   ; Possible values: true or false
   server_can_do_tls = false

   ; Server requires TLS
   ; Possible values: true or false
   server_requires_tls = false

   ; Client can do TLS
   ; Possible values: true or false
   client_can_do_tls = false

   ; Client requires TLS
   ; Possible value: true or false
   client_requires_tls = false

   ; Path to the certificate authority file
   ; E.g. ca_file = "/etc/bareos-webui/tls/BareosCA.crt"
   ;ca_file = ""

   ; Path to the cert file which needs to contain the client certificate and the key in PEM encoding
   ; E.g. ca_file = "/etc/bareos-webui/tls/restricted-named-console.pem"
   ;cert_file = ""

   ; Passphrase needed to unlock the above cert file if set
   ;cert_file_passphrase = ""

   ; Allowed common names
   ; E.g. allowed_cns = "host1.example.com"
   ;allowed_cns = ""

   ;------------------------------------------------------------------------------
   ; Section another-host-dir
   ;------------------------------------------------------------------------------
   [another-host-dir]
   enabled = "no"
   address = "192.168.120.1"
   port = 9101
   ;catalog = "MyCatalog"
   ;tls_verify_peer = false
   ;server_can_do_tls = false
   ;server_requires_tls = false
   ;client_can_do_tls = false
   ;client_requires_tls = false
   ;ca_file = ""
   ;cert_file = ""
   ;cert_file_passphrase = ""
   ;allowed_cns = ""

You can add as many directors as you want, also the same host with a different name and different catalog, if you have multiple catalogs.


Configure your :file:`/etc/bareos-webui/configuration.ini`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some parameters of the |webui| can be configured in :file:`/etc/bareos-webui/configuration.ini`:

.. code-block:: bareosconfig
   :caption: /etc/bareos-webui/configuration.ini

   ;
   ; Bareos WebUI Configuration File
   ;
   ; File: /etc/bareos-webui/configuration.ini
   ;

   ;------------------------------------------------------------------------------
   ; SESSION SETTINGS
   ;------------------------------------------------------------------------------
   ;
   ;[session]
   ; Session timeout in seconds
   ; Default:
   ;timeout=3600

   ;------------------------------------------------------------------------------
   ; DASHBOARD SETTINGS
   ;------------------------------------------------------------------------------
   ;[dashboard]
   ; Autorefresh Interval in milliseconds
   ; Default:
   ;autorefresh_interval=60000

   ;------------------------------------------------------------------------------
   ; TABLE SETTINGS
   ;------------------------------------------------------------------------------
   ;[tables]
   ; Possible values for pagination
   ; Default:
   ;pagination_values=10,25,50,100

   ; Default number of rows per page
   ; for possible values see pagination_values
   ; Default:
   ;pagination_default_value=25

   ; State saving - restore table state on page reload.
   ; Default:
   ;save_previous_state=false

   ;------------------------------------------------------------------------------
   ; VARIOUS SETTINGS
   ;------------------------------------------------------------------------------
   ;[autochanger]
   ; Name of the pool used to label and assign new media, e.g. Scratch.
   ; Default:
   ;labelpooltype=

   ;[restore]
   ; Restore filetree refresh timeout in milliseconds
   ; Default:
   ;filetree_refresh_timeout=120000

   ; Merge jobs on client selection
   ; Default:
   ;merge_jobs=true

   ; Merge filesets on client selection
   ; Default:
   ;merge_filesets=true

   ;------------------------------------------------------------------------------
   ; THEME SETTINGS
   ;------------------------------------------------------------------------------
   ;[theme]
   ; Possible values: default, sunflower
   ; Default:
   ;name=sunflower

   ;------------------------------------------------------------------------------
   ; EXPERIMENTAL FEATURES
   ;------------------------------------------------------------------------------
   ;[experimental]
   ; Configuration resource graph
   ; Default:
   ;configuration_resource_graph=false

.. _section-updating-bvfs-cache-frequently:

Configure updating the Bvfs cache frequently
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The restore module in the |webui| makes use of the Bvfs API and for example the :bcommand:`.bvfs_update` command to
generate or update the Bvfs cache for jobs that are not already in the cache.

In case of larger backup jobs with lots of files that are not already in the cache, this could lead to timeouts
while trying to load the filetree in the |webui|. That is why we highly recommend to update the Bvfs cache frequently.

This can be accomplished by the Run Script directive of a Job Resource.

The following code snippet is an example how to run the cache update process in a RunScript after the catalog backup.

.. code-block:: bareosconfig

   Job {
     Name = "BackupCatalog"
     Level = Full
     Fileset = "Catalog"
     Schedule = "WeeklyCycleAfterBackup"
     JobDefs = "DefaultJob"
     WriteBootstrap = "|/usr/sbin/bsmtp -h localhost -f "(Bareos) " -s "Bootstrap for Job %j" root@localhost"
     Priority = 100
     run before job = "/usr/lib/bareos/scripts/make_catalog_backup MyCatalog"
     run after job = "/usr/lib/bareos/scripts/delete_catalog_backup MyCatalog"
     Run Script {
       Console = ".bvfs_update"
       RunsWhen = After
       RunsOnClient = No
     }

.. note::

   We do not provide a list of Jobs specified in the *JobId* command argument so the cache is computed
   for all jobs not already in the cache.

As an alternative to the method above the Bvfs cache can be updated after each job run by using the Run Script directive as well.

.. code-block:: bareosconfig

   Job {
     Name = "backup-client-01"
     Client = "client-01.example.com"
     JobDefs = "DefaultJob"
     Run Script {
       Console = ".bvfs_update jobid=%i"
       RunsWhen = After
       RunsOnClient = No
     }
   }

.. note::

   We do provide a specific JobId in the *JobId* command argument in this example. Only the *JobId* given by the placeholder %i will be computed into the cache.



NGINX
~~~~~

.. index::
   single: nginx; bareos-webui

If you prefer to use |webui| on Nginx with php-fpm instead of Apache, a basic working configuration could look like this:

.. code-block:: bareosconfig
   :caption: bareos-webui on nginx

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

                   # php5-cgi alone:
                   # pass the PHP
                   # scripts to FastCGI server
                   # listening on 127.0.0.1:9000
                   #fastcgi_pass 127.0.0.1:9000;

                   # php5-fpm:
                   fastcgi_pass unix:/var/run/php5-fpm.sock;

                   # APPLICATION_ENV:  set to 'development' or 'production'
                   #fastcgi_param APPLICATION_ENV development;
                   fastcgi_param APPLICATION_ENV production;

           }

   }

This will make the |webui| accessible at http://bareos:9100/ (assuming your DNS resolve the hostname :strong:`bareos` to the NGINX server).

php.ini settings
~~~~~~~~~~~~~~~~

-  The |webui| relies on date functions. Set the date.timezone directive according to the timezone of your |dir|.

   .. code-block:: php

      [Date]
      ; Defines the default timezone used by the date functions
      ; http://php.net/date.timezone
      date.timezone = Europe/Berlin



.. _section-webui-command-acl-requirements:

Command ACL Requirements
------------------------

The following tables show which commands are required and optional for each
module of the |webui|.

Optional commands may be denied by :config:option:`dir/profile/CommandACL`
settings to limit specific functionality. If you deny a required command,
the module will not work.

.. note::

   The commands :bcommand:`.api`, :bcommand:`.help` and :bcommand:`use` are essential commands
   and should never be denied by :config:option:`dir/profile/CommandACL` settings in your
   :config:option:`dir/console` or :config:option:`dir/profile` resources.

.. csv-filter:: Client Module
   :header-rows: 1
   :included_cols: 0,1
   :exclude: {1: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Dashboard Module
   :header-rows: 1
   :included_cols: 0,3
   :exclude: {3: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Director Module
   :header-rows: 1
   :included_cols: 0,4
   :exclude: {4: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Fileset Module
   :header-rows: 1
   :included_cols: 0,5
   :exclude: {5: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Job Module
   :header-rows: 1
   :included_cols: 0,6
   :exclude: {6: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Volume/Media Module
   :header-rows: 1
   :included_cols: 0,7
   :exclude: {7: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Pool Module
   :header-rows: 1
   :included_cols: 0,8
   :exclude: {8: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Restore Module
   :header-rows: 1
   :included_cols: 0,9
   :exclude: {9: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Schedule Module
   :header-rows: 1
   :included_cols: 0,10
   :exclude: {10: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Storage Module
   :header-rows: 1
   :included_cols: 0,11
   :exclude: {11: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

.. csv-filter:: Analytics Module
   :header-rows: 1
   :included_cols: 0,12
   :exclude: {12: '-'}
   :file: ../../../../webui/module/Application/config/commands.csv

A complete overview of :command:`bconsole` command usage in the |webui| can be found in
the Developer Guide chapter ":ref:`section-dev-webui-command-usage-in-modules`".

.. _section-webui-access-control-configuration:

Access Control Configuration
----------------------------

Access Control is configured in :config:option:`dir/profile`,
:config:option:`dir/console` or :config:option:`dir/user` resources.

Below are some example profile resources that should serve you as guidance to
configure access to certain elements of the |webui| to your needs and use cases.

Full Access
~~~~~~~~~~~

No restrictions are given by :config:option:`dir/profile`,
everything is allowed. This profile is included in the |webui| package.

.. code-block:: cfg
   :caption: Profile Resource - Administrator Access Example

   Profile {
      Name = "webui-admin"
      CommandACL = *all*
      JobACL = *all*
      ScheduleACL = *all*
      CatalogACL = *all*
      PoolACL = *all*
      StorageACL = *all*
      ClientACL = *all*
      FilesetACL = *all*
      WhereACL = *all*
   }

Limited Access
~~~~~~~~~~~~~~~

Users with the following profile example have limited access to various resources
but they are allowed to :bcommand:`run`, :bcommand:`rerun` and :bcommand:`cancel`
the jobs :strong:`backup-bareos-fd` and :strong:`backup-example-fd`.

.. note::

   Access to depending resources for the jobs set in the
   :config:option:`dir/profile/JobACL` needs also be given by
   :config:option:`dir/profile/ClientACL`,
   :config:option:`dir/profile/PoolACL`,
   :config:option:`dir/profile/StorageACL` and
   :config:option:`dir/profile/FileSetACL` settings.

Users of this profile are also able to do a restore from within the |webui| by
having access to the RestoreFiles job resource, the required Bvfs API commands
and the :bcommand:`restore` command itself.

.. code-block:: cfg
   :caption: Profile Resource - Limited Access Example

   Profile {
      Name = "webui-user"
      # Multiple CommandACL directives as given below are concatenated
      CommandACL = .api, .help, use, version, status, show
      CommandACL = list, llist
      CommandACL = run, rerun, cancel, restore
      CommandACL = .clients, .jobs, .filesets, .pools, .storages, .defaults, .schedule
      CommandACL = .bvfs_update, .bvfs_get_jobids, .bvfs_lsdirs, .bvfs_lsfiles
      CommandACL = .bvfs_versions, .bvfs_restore, .bvfs_cleanup
      JobACL = backup-bareos-fd, backup-example-fd, RestoreFiles
      ScheduleACL = WeeklyCycle
      CatalogACL = MyCatalog
      PoolACL = Full, Differential, Incremental
      StorageACL = File
      ClientACL = bareos-fd, example-fd
      FilesetACL = SelfTest, example-fileset
      WhereACL = *all*
   }

Read-Only Access
~~~~~~~~~~~~~~~~

This example profile resource denies access to most of the commands and
additionally restricts access to certain other resources like
:config:option:`dir/Job`, :config:option:`dir/Schedule`,
:config:option:`dir/Pool`, :config:option:`dir/Storage`,
:config:option:`dir/Client`, :config:option:`dir/Fileset`, etc.

Users of this profile would not be able to run or restore jobs, execute volume
and autochanger related operations, enable or disable resources besides other
restrictions.

.. code-block:: cfg
   :caption: Profile Resource - Read-Only Access Example 1

   Profile {
     Name = "webui-user-readonly-example-1"

     # Deny general command access
     CommandACL = !.bvfs_clear_cache, !.exit, !configure, !purge, !prune, !reload
     CommandACL = !create, !update, !delete, !disable, !enable
     CommandACL = !show, !status

     # Deny job related command access
     CommandACL = !run, !rerun, !restore, !cancel

     # Deny autochanger related command access
     CommandACL = !mount, !umount, !unmount, !export, !import, !move, !release, !automount

     # Deny media/volume related command access
     CommandACL = !add, !label, !relabel, !truncate

     # Deny SQL related command access
     CommandACL = !sqlquery, !query, !.sql

     # Deny debugging related command access
     CommandACL = !setdebug, !trace

     # Deny network related command access
     CommandACL = !setbandwidth, !setip, !resolve

     # Allow non-excluded command access
     CommandACL = *all*

     # Allow access to the following job resources
     Job ACL = backup-bareos-fd, RestoreFiles

     # Allow access to the following schedule resources
     Schedule ACL = WeeklyCycle

     # Allow access to the following catalog resources
     Catalog ACL = MyCatalog

     # Deny access to the following pool resources
     Pool ACL = !Scratch

     # Allow access to non-excluded pool resources
     Pool ACL = *all*

     # Allow access to the following storage resources
     Storage ACL = File

     # Allow access to the following client resources
     Client ACL = bareos-fd

     # Allow access to the following filset resources
     FileSet ACL = SelfTest

     # Allow access to restore to any filesystem location
     Where ACL = *all*
   }

Alternatively the example above can be configured as following if you prefer a
shorter version.

.. code-block:: cfg
   :caption: Profile Resource - Read-Only Access Example 2

   Profile {
     Name = "webui-user-readonly-example-2"

     # Allow access to the following commands
     CommandACL = .api, .help, use, version, status
     CommandACL = list, llist
     CommandACL = .clients, .jobs, .filesets, .pools, .storages, .defaults, .schedule
     CommandACL = .bvfs_lsdirs, .bvfs_lsfiles, .bvfs_update, .bvfs_get_jobids, .bvfs_versions, .bvfs_restore

     # Allow access to the following job resources
     Job ACL = backup-bareos-fd, RestoreFiles

     # Allow access to the following schedule resources
     Schedule ACL = WeeklyCycle

     # Allow access to the following catalog resources
     Catalog ACL = MyCatalog

     # Allow access to the following  pool resources
     Pool ACL = Full, Differential, Incremental

     # Allow access to the following storage resources
     Storage ACL = File

     # Allow access to the following client resources
     Client ACL = bareos-fd

     # Allow access to the following filset resources
     FileSet ACL = SelfTest

     # Allow access to restore to any filesystem location
     Where ACL = *all*
   }

For more details, please read :ref:`DirectorResourceProfile`.

.. _section-webui-restore:

Restore
-------

By default when running a restore in the |webui| the most recent version of all files from the available backups will be restored. You can change this behavior by selecting the merge strategy and specific job selections in the fields described below. The |webui| allows you to restore multiple files or specific file versions.

.. _section-webui-restore-parameters:

Available restore parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-0.*
   :width: 80%

Client

   A list of available backup clients.


Backup jobs

   A list of successful backup jobs available for the selected client.


Merge all client filesets

   Determines if all available backup job filesets for the selected client should be merged into one file tree. This is helpful i.e. if multiple backup jobs with different filesets are available for the selected client. When you are just interested in a specific backup job, disable merging here and make the appropriate selection of a backup job.


Merge all related jobs to last full backup of selected backup job

   By default all most recent versions of a file from your incremental, differential and full backup jobs will be merged into the file tree. If this behaviour is not desirable and instead the file tree should show the contents of a particular backup job, set the value to "No" here. Select a specific backup job afterwards to browse through the according file tree which has been backed up by that job.


Restore to client

   In case you do not want to restore to the original client, you can select an alternative client here.


Restore job

   Sometimes dedicated restore jobs may be required, which can be selected here.


Replace files on client

   Here you can change the behaviour of how and when files should be replaced on the backup client while restoring.

      - always
      - never
      - if file being restored is older than existing file
      - if file being restored is newer than existing file


Restore location on client

   If you like to restore all files to the original location then enter a single :file:`/` here but keep the settings of "Replace files on client" in mind.

   In case you want to use another location, simply enter the path here where you want to restore to on the selected client, for example :file:`/tmp/bareos-restore/`.


Plugin options

   Provide a plugin options string here if required. The field is only shown if a fileset using a plugin is detected.


Restore multiple files
~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-1.*
   :width: 80%

Restore a specific file version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: /include/images/bareos-webui-restore-2.*
   :width: 80%

Limitations
~~~~~~~~~~~

.. note::

   Restoring NDMP backups is currently not supported by Bareos WebUI.
   Please use the bconsole instead.

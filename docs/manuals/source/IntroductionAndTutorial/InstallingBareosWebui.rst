.. _section-webui:

.. _section-install-webui:

Installing Bareos Webui
=======================

:index:`\ <single: Webui>`
:index:`\ <single: Webui; Install>`

This chapter addresses the installation process of the |webui|.

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

   -  Client, Director, Storage, and Scheduler status

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

-  |dir| version >= |webui| version

-  The |webui| can be installed on any host. It does not have to be installed on the same as the |dir|.

-  The default installation uses an Apache webserver with mod-rewrite, mod-php and mod-setenv.

-  PHP >= 5.3.23

-  On SUSE Linux Enterprise 12 you need the additional SUSE Linux Enterprise Module for Web Scripting 12.

Version < 16.2
~~~~~~~~~~~~~~

|webui| :sinceVersion:`16.2.4: bareos-webui incorporates Zend Framework 2` incorporates the required Zend Framework 2 components, no extra Zend Framework installation is required. For older versions of **bareos-webui**, you must install Zend Framework separately. Unfortunately, not all distributions offer Zend Framework 2 packages. The following list shows where to get the Zend Framework 2 package:

-  RHEL, CentOS

   -  https://fedoraproject.org/wiki/EPEL

   -  https://apps.fedoraproject.org/packages/php-ZendFramework2

-  Fedora

   -  https://apps.fedoraproject.org/packages/php-ZendFramework2

-  SUSE, Debian, Ubuntu

   -  http://download.bareos.org/bareos

Also be aware, that older versions of |dir| do not support the :ref:`section-SubdirectoryConfigurationScheme` and therefore Bareos configuration resource files must be included manually.

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

Minimal Configuration
~~~~~~~~~~~~~~~~~~~~~

This assumes, |dir| and |webui| are installed on the same host.

#. If you are using SELinux, allow HTTPD scripts and modules make network connections:

   .. code-block:: shell-session

      setsebool -P httpd_can_network_connect on

   For details, see :ref:`section-webui-selinux`.

#. Restart Apache (to load configuration provided by bareos-webui, see :ref:`section-webui-apache`)

#. Use :command:`bconsole` to create a user with name **admin** and password **secret** and permissions defined in :config:option:`Dir/Profile = webui-admin`\ :

   .. code-block:: bconsole

      *<input>reload</input>
      reloaded
      *<input>configure add console name=admin password=secret profile=webui-admin tlsenable=false</input>

   Of course, you can choose other names and passwords. For details, see :ref:`section-webui-console`.

#. Login to http://HOSTNAME/bareos-webui with username and password as created.

Configuration Details
~~~~~~~~~~~~~~~~~~~~~

.. _section-webui-console:

Create a restricted consoles
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There is not need, that |webui| itself provide a user management. Instead it uses so named :config:option:`Dir/Console`\  defined in the |dir|. You can have multiple consoles with different names and passwords, sort of like multiple users, each with different privileges.

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

For details, please read :ref:`DirectorResourceProfile`.

.. _section-webui-selinux:

SELinux
^^^^^^^

:index:`\ <single: SELinux; bareos-webui>`\ 

To use |dir| on a system with SELinux enabled, permission must be given to HTTPD to make network connections:

.. code-block:: shell-session

   setsebool -P httpd_can_network_connect on


.. _section-webui-apache:
   
Configure your Apache Webserver
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Apache; bareos-webui>`

The package **bareos-webui** provides a default configuration for Apache. Depending on your distribution, it is installed at :file:`/etc/apache2/conf.d/bareos-webui.conf`, :file:`/etc/httpd/conf.d/bareos-webui.conf` or :file:`/etc/apache2/available-conf/bareos-webui.conf`.

The required Apache modules, :strong:`setenv`, :strong:`rewrite` and :strong:`php` are enabled via package postinstall script. However, after installing the **bareos-webui** package, you need to restart your Apache webserver manually.


.. _section-webui-configuration-files:

Configure your /etc/bareos-webui/directors.ini
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Configuration; WebUI>`

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
   diraddress = "localhost"

   ; Default value is 9101
   dirport = 9101

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
   diraddress = "192.168.120.1"
   dirport = 9101
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

Configure your /etc/bareos-webui/configuration.ini
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since :sinceVersion:`16.2.2: /etc/bareos-webui/configuration.ini` you are able to configure some parameters of the |webui| to your needs.

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
   [session]
   ; Default: 3600 seconds
   timeout=3600

   ;------------------------------------------------------------------------------
   ; DASHBOARD SETTINGS
   ;------------------------------------------------------------------------------
   [dashboard]
   ; Autorefresh Interval
   ; Default: 60000 milliseconds
   autorefresh_interval=60000

   ;------------------------------------------------------------------------------
   ; TABLE SETTINGS
   ;------------------------------------------------------------------------------
   [tables]
   ; Possible values for pagination
   ; Default: 10,25,50,100
   pagination_values=10,25,50,100

   ; Default number of rows per page
   ; for possible values see pagination_values
   ; Default: 25
   pagination_default_value=25

   ; State saving - restore table state on page reload.
   ; Default: false
   save_previous_state=false

   ;------------------------------------------------------------------------------
   ; VARIOUS SETTINGS
   ;------------------------------------------------------------------------------
   [autochanger]
   ; Pooltype for label to use as filter.
   ; Default: none
   labelpooltype=scratch

Upgrade from 15.2 to 16.2
-------------------------

Console/Profile changes
~~~~~~~~~~~~~~~~~~~~~~~

The |webui| Director profile shipped with Bareos 15.2 (:config:option:`Dir/Profile = webui`\  in the file :file:`/etc/bareos/bareos-dir.d/webui-profiles.conf`) is not sufficient to use the |webui| 16.2. This has several reasons:

#. The handling of :strong:`Acl`s is more strict in Bareos 16.2 than it has been in Bareos 15.2. Substring matching is no longer enabled, therefore you need to change :bcommand:`.bvfs_*` to :bcommand:`.bvfs_.*` in your :config:option:`dir/profile/CommandAcl`\  to have a proper regular expression. Otherwise the restore module won’t work any longer, especially the file browser.

#. The |webui| 16.2 uses following additional commands:

   -  .help

   -  .schedule

   -  .pools

   -  import

   -  export

   -  update

   -  release

   -  enable

   -  disable

If you used an unmodified :file:`/etc/bareos/bareos-dir.d/webui-profiles.conf` file, the easiest way is to overwrite it with the new profile file :file:`/etc/bareos/bareos-dir.d/profile/webui-admin.conf`. The new :config:option:`Dir/Profile = webui-admin`\  allows all commands, except of the dangerous ones, see :ref:`section-webui-profile`.

directors.ini
~~~~~~~~~~~~~

Since :sinceVersion:`16.2.0: Webui offers limited support for multiple catalogs` it is possible to work with different catalogs. Therefore the catalog parameter has been introduced. If you don’t set a catalog explicitly the default :config:option:`Dir/Catalog = MyCatalog`\  will be used. Please see :ref:`section-webui-configuration-files` for more details.

configuration.ini
~~~~~~~~~~~~~~~~~

Since 16.2 the |webui| introduced an additional configuration file besides the directors.ini file named configuration.ini where you are able to adjust some parameters of the webui to your needs. Please see :ref:`section-webui-configuration-files` for more details.

Additional information
----------------------

NGINX
~~~~~

:index:`\ <single: nginx; bareos-webui>`\ 

If you prefer to use |webui| on Nginx with php5-fpm instead of Apache, a basic working configuration could look like this:

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



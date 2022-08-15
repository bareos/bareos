.. _InstallChapter:

Installing Bareos
=================

.. index::
   pair: Bareos; Installation
   pair: Installation; Linux

If you are like me, you want to get Bareos running immediately to get a feel for it, then later you want to go back and read about all the details. This chapter attempts to accomplish just that: get you going quickly without all the details.

Bareos comes prepackaged for a number of Linux distributions. So the easiest way to get to a running Bareos installation, is to use a platform where prepacked Bareos packages are available. Additional information can be found in the chapter :ref:`Operating Systems <SupportedOSes>`.

If Bareos is available as a package, only 4 steps are required to get to a running Bareos system:

#.

   :ref:`section-AddSoftwareRepository`

#.

   :ref:`section-InstallBareosPackages`

#.

   :ref:`section-CreateDatabase`

#.

   :ref:`section-StartDaemons`

This will start a very basic Bareos installation which will regularly backup a directory to disk. In order to fit it to your needs, you’ll have to adapt the configuration and might want to backup other clients.

.. _section-AddSoftwareRepository:

Decide about the Bareos release to use
--------------------------------------

You’ll find Bareos stable release binaries packages repositories at https://download.bareos.org/bareos/release.
For those who have a valid subscription contract, the repositories are located at https://download.bareos.org/bareos/release/.

The public key to verify the repository is also in repository directory (:file:`Release.key` for Debian based distributions, :file:`repodata/repomd.xml.key` for RPM based distributions).

Section :ref:`section-InstallBareosPackages` describes how to add the software repository to your system.

Bareos requires a PostgreSQL database as its catalog.
The backend is contained in package **bareos-database-postgresql**.
The Bareos database packages have their dependencies only to the database client packages, therefore the database itself must be installed manually.


.. _section-InstallBareosPackages:

Install the Bareos Software Packages
------------------------------------

The package **bareos** is only a meta package which contains dependencies on the main components of Bareos, see :ref:`section-BareosPackages`. If you want to setup a distributed environment (like one Director, separate database server, multiple Storage daemons) you have to choose the regarding Bareos packages to install on each of the hosts instead of just installing the **bareos** package.

The following code snippets are shell scripts that can be used as orientation how to download the package repositories and install bareos. The release version number for **bareos** and the corresponding Linux distribution have to be updated for your needs, respectively.

**See** https://download.bareos.org/bareos/release/ or https://download.bareos.com/bareos/release/ (subscription only) **for applicable releases and distributions.**
See :ref:`section-UniversalLinuxClient` if your distribution is not present.

To use the following code snippet if you have a subscription, you can declare your credentials


Install on RedHat based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RHEL, CentOS, Fedora
^^^^^^^^^^^^^^^^^^^^

.. index::
   single: Platform; RHEL
   single: Platform; CentOS
   single: Platform; Fedora
   single: Platform; EL

Bareos :sinceVersion:`15.2.0: requires: jansson` requires the Jansson library package.
On RHEL 7 it is available through the RHEL Server Optional channel.

The EL_8 repository is intended for RHEL 8 derivates,
like AlmaLinux, CentOS Stream, Oracle and Rocky Linux. Same rules apply for EL_9.


.. code-block:: sh
   :caption: Shell example script for Bareos installation on RHEL / EL / Fedora

   #!/bin/sh

   # Declare your credentials for subscription if you have
   # BAREOS_USER=info_at_domain.example
   # BAREOS_PASSWORD=Super#Encrypted/Password

   # See https://download.bareos.org/bareos/release/
   # or https://download.bareos.com/bareos/release/
   # for applicable releases and distributions

   DIST=EL_9
   # or
   # DIST=RHEL_9
   # DIST=RHEL_8
   # DIST=EL_9
   # DIST=EL_8
   # DIST=RHEL_7
   # DIST=CentOS_7
   # DIST=Fedora_36
   # DIST=Fedora_35

   RELEASE=release/21
   # RELEASE=experimental/nightly

   # add the Bareos repository
   URL=https://download.bareos.org/bareos/$RELEASE/$DIST
   # or https://${USER}:${PASSWORD}@download.bareos.com/bareos/$RELEASE/$DIST

   wget -O /etc/yum.repos.d/bareos.repo $URL/bareos.repo

   # install Bareos packages
   yum install bareos bareos-database-postgresql


Install on SUSE based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SUSE Linux Enterprise Server (SLES), openSUSE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: Platform; SLES
   single: Platform; openSUSE

.. code-block:: sh
   :caption: Shell example script for Bareos installation on SLES / openSUSE

   #!/bin/sh

   # Declare your credentials for subscription if you have
   # BAREOS_USER=info_at_domain.tld
   # BAREOS_PASSWORD=Super#Encrypted/Password

   # See https://download.bareos.org/bareos/release/
   # or https://download.bareos.com/bareos/release/
   # for applicable releases and distributions

   DIST=SLE_15_SP4
   # or
   # DIST=SLE_15_SP3
   # DIST=SLE_12_SP5
   # DIST=openSUSE_Leap_15.4
   # DIST=openSUSE_Leap_15.3

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   # add the Bareos repository
   URL=https://download.bareos.org/bareos/$RELEASE/$DIST
   # or https://${USER}:${PASSWORD}@download.bareos.com/bareos/$RELEASE/$DIST
   zypper addrepo --refresh $URL/bareos.repo

   # install Bareos packages
   zypper install bareos bareos-database-postgresql

.. _section-InstallBareosPackagesDebian:

Install on Debian based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Debian / Ubuntu
^^^^^^^^^^^^^^^

.. index::
   single: Platform; Debian
   single: Platform; Ubuntu

Bareos :sinceVersion:`15.2.0: requires: jansson` requires the Jansson library package. On Ubuntu is it available in Ubuntu Universe. In Debian, is it included in the main repository.

.. code-block:: sh
   :caption: Shell example script for Bareos installation on Debian / Ubuntu

   #!/bin/sh

   # Declare your credentials for subscription if you have
   # BAREOS_USER=info_at_domain.tld
   # BAREOS_PASSWORD=Super#Encrypted/Password

   # Choose repository to use
   REPOSITORY=download.bareos.org
   # REPOSITORY=download.bareos.com

   # Subscriber only Setup credentials for apt auth
   #cat <<EOF>/etc/apt/auth.conf.d/bareos.conf
   #machine ${REPOSITORY} login ${BAREOS_USER} password ${BAREOS_PASSWORD}
   #EOF
   #chmod 0600 /etc/apt/auth.conf.d/bareos.conf
   ## Copy the credentials to /root/.netrc for wget usage
   #cat /etc/apt/auth.conf.d/bareos.conf >> /root/.netrc

   # See ${REPOSITORY}/bareos/release
   # for applicable releases and distributions

   DIST=Debian_11
   # or
   # DIST=Debian_10
   # DIST=xUbuntu_22.04
   # DIST=xUbuntu_20.04
   # DIST=xUbuntu_18.04

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   # declare the Bareos repository
   URL=https://${REPOSITORY}/bareos/$RELEASE/$DIST

   # add the Bareos repository
   cat <<EOF>/etc/apt/sources.list.d/bareos.list
   deb [signed-by=/etc/keyrings.d/bareos.gpg] ${URL} /
   EOF

   # add package key
   mkdir -p /etc/keyrings.d
   wget -q $URL/Release.key -O- | gpg --dearmor -o /etc/keyrings.d/bareos.gpg

   # install Bareos packages
   apt-get update
   apt-get install bareos bareos-database-postgresql

If you use the versions of Bareos directly integrated into the distributions, please note that there are some differences, see :ref:`section-DebianOrgLimitations`.

.. _section-DebianOrg:
.. _section-DebianOrgLimitations:

Debian.org / Ubuntu Universe
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: Platform; Debian; Debian.org
   single: Platform; Ubuntu; Universe

Some Debian releases (8-10) include an old version of Bareos (Bareos <= 16.2). Some releases of Ubuntu Universe did also include these packages.

These packages have some limitations and are outdated. Nowadays, they shouldn't be used.

.. _section-FreeBSD:

Install on FreeBSD based Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; FreeBSD

.. code-block:: sh
   :caption: Shell example script for Bareos installation on FreeBSD

   #!/bin/sh

   # Declare your credentials for subscription if you have
   # USER=info_at_domain.sample
   # PASSWORD=Super#Encrypted/Password

   # See https://download.bareos.org/bareos/release/
   # or https://download.bareos.com/bareos/release/
   # for applicable releases and distributions

   DIST=FreeBSD_13.0
   # or
   # DIST=FreeBSD_12.2

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   # add the Bareos repository
   URL=https://download.bareos.org/bareos/$RELEASE/$DIST
   # or https://${USER}:${PASSWORD}@download.bareos.com/bareos/$RELEASE/$DIST

   # add the Bareos repository
   mkdir -p /usr/local/etc/pkg/repos
   cd /usr/local/etc/pkg/repos
   wget -q $URL/bareos.conf

   # install Bareos packages
   pkg install --yes bareos.com-director bareos.com-storage bareos.com-filedaemon bareos.com-database-postgresql bareos.com-bconsole

   # setup the Bareos database
   su postgres -c /usr/lib/bareos/scripts/create_bareos_database
   su postgres -c /usr/lib/bareos/scripts/make_bareos_tables
   su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges

   # enable services
   sysrc bareosdir_enable=YES
   sysrc bareossd_enable=YES
   sysrc bareosfd_enable=YES

   # start services
   service bareos-dir start
   service bareos-sd start
   service bareos-fd start


.. _section-Solaris:

Install on Oracle Solaris
~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; Solaris

Bareos offers **IPS** (*Image Packaging System*) filedaemon Packages for **Oracle Solaris 11.4**.

First, download the Solaris package to the local disk and add the package as publisher
**bareos**:

.. code-block:: shell-session
   :caption: Add bareos publisher

   root@solaris114:~# pkg set-publisher -p bareos-fd-<version>.p5p  bareos
   pkg set-publisher:
     Added publisher(s): bareos


Then, install the filedaemon with **pkg install**:


.. code-block:: shell-session
   :caption: Install solaris package

   root@solaris114:~# pkg install bareos-fd
             Packages to install:  1
              Services to change:  1
         Create boot environment: No
   Create backup boot environment: No

   DOWNLOAD                                PKGS         FILES    XFER (MB)   SPEED
   Completed                                1/1         44/44      1.0/1.0  4.8M/s

   PHASE                                          ITEMS
   Installing new actions                         94/94
   Updating package state database                 Done
   Updating package cache                           0/0
   Updating image state                            Done
   Creating fast lookup database                working |


After installation, check the bareos-fd service status with **svcs bareos-fd**:

.. code-block:: shell-session
   :caption: Check solaris service

   root@solaris114:~# svcs bareos-fd
   STATE          STIME      FMRI
   online         16:16:14   svc:/bareos-fd:default


Finish the installation by adapting the configuration in :file:`/usr/local/etc/bareos` and restart the
service with **svcadm restart bareos-fd**:

.. code-block:: shell-session
   :caption: Restart solaris service

   root@solaris114:~# svcadm restart bareos-fd

The bareos filedaemon service on solaris is now ready for use.

.. _section-macosx:

Install on Mac OS X
~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; Mac OS X

Bareos for MacOS X is available either

-  via the `Homebrew project <https://brew.sh/>`_ (https://formulae.brew.sh/formula/bareos-client) or

-  as pkg file from https://download.bareos.org/bareos/release/.

However, you have to choose upfront, which client you want to use. Otherwise conflicts do occur.

Both packages contain the |fd| and :command:`bconsole`.

Installing the Bareos Client as PKG
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Installation; MacOS>`\

The Bareos installer package for Mac OS X contains the |fd| for Mac OS X 10.5 or later.

On your local Mac, you must be an admin user. The main user is an admin user.

Download the :file:`bareos-*.pkg` installer package from https://download.bareos.org/bareos/release/.

Find the .pkg you just downloaded. Install the .pkg by holding the CTRL key, left-clicking the installer and choosing "open".

Follow the directions given to you and finish the installation.

Configuration
^^^^^^^^^^^^^

To make use of your |fd| on your system, it is required to configure the |dir| and the local |fd|.

Configure the server-side by follow the instructions at :ref:`section-AddAClient`.

After configuring the server-side you can either transfer the necessary configuration file using following command or configure the client locally.

Option 1: Copy the director resource from the Bareos Director to the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Assuming your client has the DNS entry :strong:`client2.example.com` and has been added to |dir| as :config:option:`dir/client = client2-fd`\ :

.. code-block:: shell-session

   scp /etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf root@client2.example.com:/usr/local/etc/bareos/bareos-fd.d/director/

This differs in so far, as on Linux the configuration files are located under :file:`/etc/bareos/`, while on MacOS they are located at :file:`/usr/local/etc/bareos/`.

Option 2: Edit the director resource on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Alternatively, you can edit the file :file:`/usr/local/etc/bareos/bareos-fd.d/director/bareos-dir.conf`.

This can be done by right-clicking the finder icon in your task bar, select "Go to folder ..." and paste :file:`/usr/local/etc/bareos/bareos-fd.d/director/`.

Select the :file:`bareos-dir.conf` file and open it.

Alternatively you can also call following command on the command console:

.. code-block:: shell-session

   open -t /usr/local/etc/bareos/bareos-fd.d/director/bareos-dir.conf

The file should look similar to this:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = bareos-dir
     Password = "SOME_RANDOM_PASSWORD"
     Description = "Allow the configured Director to access this file daemon."
   }

Set this client-side password to the same value as given on the server-side.


.. warning::

   The configuration file contains passwords and therefore must not be accessible for any users except admin users.

Restart bareos-fd after changing the configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The bareos-fd must be restarted to reread its configuration:

.. code-block:: shell-session
   :caption: Restart the |fd|

   sudo launchctl stop  org.bareos.bareos-fd
   sudo launchctl start org.bareos.bareos-fd

Verify that the Bareos File Daemon is working
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Open the :command:`bconsole` on your |dir| and check the status of the client with

.. code-block:: bareosconfig

   *<input>status client=client2-fd</input>

In case, the client does not react, following command are useful the check the status:

.. code-block:: shell-session
   :caption: Verify the status of |fd|

   # check if bareos-fd is started by system:
   sudo launchctl list org.bareos.bareos-fd

   # get process id (PID) of bareos-fd
   pgrep bareos-fd

   # show files opened by bareos-fd
   sudo lsof -p `pgrep bareos-fd`

   # check what process is listening on the |fd| port
   sudo lsof -n -iTCP:9102 | grep LISTEN

You can also manually start bareos-fd in debug mode by:

.. code-block:: shell-session
   :caption: Start |fd| in debug mode

   sudo /usr/local/sbin/bareos-fd -f -d 100


.. _section-UniventionCorporateServer:

Install on Univention Corporate Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; Univention Corporate Server

The Bareos version for the Univention App Center integraties into the Univention Enterprise Linux environment, making it easy to backup all the systems managed by the central Univention Corporate Server.

If you are not interested in this additional functionality, the commands described in :ref:`section-InstallBareosPackagesDebian` will also work for Univention Corporate Servers.

Preamble
^^^^^^^^

The `Univention Corporate Server <https://www.univention.de/>`_ is an enterprise Linux distribution based on Debian. It consists of an integrated management system for the centralised administration of servers, computer workplaces, users and their rights as well as a wide range of server applications. It also includes an Unvention App Center for the easy installation and management of extensions and appliances.

Bareos is part of the `App Center <https://www.univention.de/produkte/univention-app-center/app-katalog/bareos/>`_ and therefore an Univention environment can easily be extended to provide backup functionality for the Univention servers as well as for the connected client systems. Using the Univention Management Console (UMC), you can also create backup jobs for client computers (Windows or Linux systems), without the need of editing configuration files.

The Bareos Univention App is shipped with a default configuration for the director daemon and the storage daemon.


.. warning::

   You need to review some Univention configuration registry (UCR) variables. Most likely, you will want to set the location where the backups are stored. Otherwise, you may quickly run out of disk space on your backup server!

You will find further information under :ref:`section-UniventionBackupStorage`.

Quick Start
^^^^^^^^^^^

-  Determine the space requirements and where to store your backup data

-  Set the :strong:`bareos/*` UCR variables according to your needs, see :ref:`section-UCR`

-  Restart :command:`bareos-dir`, :command:`bareos-sd` and :command:`bareos-fd` (or simply reboot the server)

-  Install the Bareos file daemon on clients and copy Director configuration resource file from

   - :file:`/etc/bareos/bareos-dir-export/client/<clientname>-fd/bareos-fd.d/director/*.conf`

   -  (or :file:`/etc/bareos/autogenerated/client-configs/<hostname>.conf`, if Bareos < 16.2.0)

   For details, see :ref:`section-UniventionAddClient`.

-  Enable backup jobs for clients in the Univention Management Console


.. _section-UCR:

UCR variables
^^^^^^^^^^^^^

:strong:`bareos/filestorage`
   : /var/lib/bareos/storage (default)

   -  Location where to store the backup files. Make sure, it offers enough disk space for a configured backup volumes.

:strong:`bareos/max_full_volume_bytes`
   : 20 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Full`\  backup pool

:strong:`bareos/max_full_volumes`
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Full`\  backup pool

:strong:`bareos/max_diff_volume_bytes`
   : 10 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Differential`\  backup pool

:strong:`bareos/max_diff_volumes`
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Differential`\  backup pool

:strong:`bareos/max_incr_volume_bytes`
   : 1 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Incremental`\  backup pool

:strong:`bareos/max_incr_volumes`
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Incremental`\  backup pool

:strong:`bareos/backup_myself`
   : no (default)

   no
      don’t backup the server itself

   yes
      backup the server itself

:strong:`bareos/webui/console/user1/username`
   : Administrator (default)

   -  User name to login at the bareos-webui

:strong:`bareos/webui/console/user1/password`
   : (no default value)

   -  Password to login at the bareos-webui

UCR variables can be set via the Univention Configuration Registry Web interface

.. image:: /include/images/univention-configuration-registry-settings.*
   :width: 100.0%



or using the :command:`ucr` command line tool:

.. code-block:: shell-session
   :caption: Enable backup of the server itself

   root@ucs:~# ucr set bareos/backup_myself=yes
   Setting bareos/backup_myself
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.



.. warning::

   univention-bareos < 15.2 did require a manual reload/restart of the bareos-dir service:

.. code-block:: shell-session
   :caption: let bareos-dir reload its configuration

   root@ucs:~# service bareos-dir reload
   [ ok ] Reloading Bareos Director: bareos-dir.

Setup
^^^^^

After installation of the Bareos app, Bareos is ready for operation. A default configuration is created automatically.

Bareos consists of three daemons called :command:`director` (or :command:`bareos-dir`), :command:`storage-daemon` (or :command:`bareos-sd`) and :command:`filedaemon` (or :command:`bareos-fd`). All three daemons are started right after the installation by the Univention App Center.

If you want to enable automatic backups of the server, you need to set the Univention configuration registry (UCR) variable :strong:`bareos/backup_myself` to :strong:`yes` and reload the director daemon.

Administration
^^^^^^^^^^^^^^

For general tasks the :ref:`bareos-webui <section-webui>` can be used. Additional, there is the :command:`bconsole` command line tool:

.. code-block:: shell-session
   :caption: Starting the bconsole

   root@ucs:~# bconsole
   Connecting to Director ucs:9101
   1000 OK: ucs-dir Version: 15.2.2 (15 November 2015)
   Enter a period to cancel a command.
   *

For general information, see the :ref:`Bconsole Tuturial <section-TuturialBconsole>`.

Backup Schedule
^^^^^^^^^^^^^^^

As a result of the default configuration located at the :command:`bareos-dir`, the backup schedule will look as follows:

Full Backups
   -  are written into the :config:option:`dir/pool = Full`\  pool

   -  on the first saturday at 21:00 o’clock

   -  and kept for 365 days

Differential Backups
   -  are written into the :config:option:`dir/pool = Differential`\  pool

   -  on every 2nd to 5th saturday at 21:00 o’clock

   -  and kept for 90 days

Incremental Backups
   -  are written into the :config:option:`dir/pool = Incremental`\  pool

   -  on every day from monday to friday at 21:00 o’clock

   -  and kept for 30 days

That means full backups will be written every first saturday at 21:00 o’clock, differential backups every 2nd to 5th saturday at 21:00 o’clock and incremental backups from monday to friday at 21:00 o’clock. So you have got one full backup every month, four weekly differential and 20 daily incremental backups per month.

This schedule is active for the Univention server backup of itself and all other clients, which are backed up through the :command:`bareos-dir` on the Univention server.

There is also a special backup task, which is the Bareos backups itself for a possible disaster recovery. This backup has got its own backup cycle which starts after the main backups. The backup consists of a database backup for the metadata of the Bareos backup server and a backup of the Bareos configuration files under :file:`/etc/bareos/`.

Backup data management
^^^^^^^^^^^^^^^^^^^^^^

Data from the backup jobs is written to volumes, which are organized in pools (see chapter :ref:`DirectorResourcePool`).

The default configuration uses three different pools, called :config:option:`dir/pool = Full`\ , :config:option:`dir/pool = Differential`\  and :config:option:`dir/pool = Incremental`\ , which are used for full backups, differential and incremental backups, respectively.

If you change the UCR variables, the configuration files will be rewritten automatically. After each change you will need to reload the director daemon.

.. code-block:: shell-session
   :caption: Example for changing the Full pool size to $10 \ast 20$ GB

   root@ucs:~# ucr set bareos/max_full_volumes=10
   Setting bareos/max_full_volumes
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.
   root@ucs:~# ucr set bareos/max_full_volume_bytes=20
   Setting bareos/max_full_volume_bytes
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.



.. warning::

   This only affects new volumes. Existing volumes will not change there size.


.. _section-UniventionBackupStorage:

Backup Storage
^^^^^^^^^^^^^^

.. warning::

   Using the default configuration, Bareos will store backups on your local disk. You may want to store the data to another location to avoid using up all of your disk space.

The location for backups is :file:`/var/lib/bareos/storage` in the default configuration.

For example, to use a NAS device for storing backups, you can mount your NAS volume via NFS on :file:`/var/lib/bareos/storage`. Alternatively, you can mount the NAS volume to another directory of your own choice, and change the UCR variable :strong:`bareos/filestorage` to the corresponding path. The directory needs to be writable by user **bareos**.

.. code-block:: shell-session
   :caption: Example for changing the storage path

   root@ucs:/etc/bareos# ucr set bareos/filestorage=/path/to/your/storage
   Setting bareos/filestorage
   File: /etc/bareos/...



.. warning::

   You need to restart the Bareos storage daemon after having changed the storage path:

.. code-block:: shell-session

   root@ucs:/# service bareos-sd restart


Bareos Webui Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^

After installation you just need to setup your login credentials via UCR variables. Therefore, set the Univention configuration registry (UCR) variable :strong:`bareos/webui/console/user1/username` and :strong:`bareos/webui/consoles/user1/password` according to your needs. The director configuration is automatically reloaded if one of those two variables changes.

Alternatively you can also set those UCR variables via commandline.

.. code-block:: shell-session
   :caption: Example for changing webui login credentials

   root@ucs:~# ucr set bareos/webui/console/user1/username="bareos"
   Setting bareos/webui/console/user1/username
   File: /etc/bareos/...
   [ ok ] Reloading Bareos Director: bareos-dir.
   root@ucs:~# ucr set bareos/webui/console/user1/password="secret"
   Setting bareos/webui/console/user1/password
   File: /etc/bareos/...
   [ ok ] Reloading Bareos Director: bareos-dir.

When your login credentials are set, you can login into Bareos Webui by following the entry in your Administration UCS Overview or directly via https://UCS_SERVER/bareos-webui/.

.. image:: /include/images/univention-ucs-overview-administration.*
   :width: 80.0%



.. _section-UniventionAddClient:

Add a client to the backup
^^^^^^^^^^^^^^^^^^^^^^^^^^

Overview
''''''''

-  Install the Bareos client software on the target system, see :ref:`Adding a Bareos Client <SecondClient>`

-  Use the Univention Management Console to add the client to the backup, see the screenshot below

-  Copy the filedaemon resource configuration file from the Univention server to the target system

Bareos >= 16.2.4
''''''''''''''''

Server-side


The Univention Bareos application comes with an automatism for the client and job configuration. If you want to add a client to the Bareos director configuration, you need use the Univention Management Console, select the client you want to backup and set the :strong:`enable backup job` checkbox to true, as shown in the screenshot below.

.. image:: /include/images/univention-client-job-activation.*
   :width: 80.0%




If the name of the client is **testw1.example.com**, corresponding configuration files will be generated:

- :file:`/etc/bareos/autogenerated/clients/testw1.example.com.include`

- :file:`/etc/bareos/bareos-dir-export/client/testw1.example.com-fd/bareos-fd.d/director/bareos-dir.conf`

Generated configuration files under :file:`/etc/bareos/bareos-dir-export/client/` are intended for the target systems. After you have :ref:`installed the Bareos client on the target system <SecondClient>`, copy the generated client configuration over to the client and save it to following directories:

-  on Linux: :file:`/etc/bareos/bareos-fd.d/director/`

-  on Windows: :file:`C:\Program Files\Bareos\bareos-fd.d/director/`

.. code-block:: shell-session
   :caption: copy client configuration from the server to the testw1.example.com client (Linux)

   root@ucs:~# CLIENTNAME=testw1.example.com
   root@ucs:~# scp /etc/bareos/bareos-dir-export/client/${CLIENTNAME}-fd/bareos-fd.d/director/*.conf root@${CLIENTNAME}:/etc/bareos/bareos-fd.d/director/

Background
''''''''''

The settings for each job resource are defined by the template files you see below:

The files

- :file:`/etc/bareos/autogenerated/clients/generic.template`

- :file:`/etc/bareos/autogenerated/clients/windows.template`

are used as templates for new clients. For Windows clients the file :file:`windows.template` is used, the :file:`generic.template` is used for all other client types.

If you disable the Bareos backup for a client, the client will not be removed from the configuration files. Only the backup job will be set inactive.

If you add three client, your client directory will look similar to this:

.. code-block:: shell-session

   root@ucs:/etc/bareos/autogenerated/clients# ls -l
   -rw-r--r-- 1 root root 430 16. Mai 15:15 generic.template
   -rw-r----- 1 root bareos 513 21. Mai 14:46 testw1.example.com.include
   -rw-r----- 1 root bareos 518 21. Mai 14:49 testw2.example.com.include
   -rw-r----- 1 root bareos 518 16. Mai 18:17 testw3.example.com.include
   -rw-r--r-- 1 root root 439 16. Mai 15:15 windows.template

The client configuration file contains, as you can see below, the client connection and the job information:

.. code-block:: shell-session

   root@ucs:/etc/bareos/autogenerated/clients# cat testw2.example.com.include
   Client {
    Name = "testw2.example.com-fd"
    Address = "testw2.example.com"
    Password = "DBLtVnRKq5nRUOrnB3i3qAE38SiDtV8tyhzXIxqR"
   }

   Job {
    Name = "Backup-testw2.example.com" # job name
    Client = "testw2.example.com-fd" # client name
    JobDefs = "DefaultJob" # job definition for the job
    FileSet = "Windows All Drives" # FileSet (data which is backed up)
    Schedule = "WeeklyCycle" # schedule for the backup tasks
    Enabled = "Yes" # this is the resource which is toggled on/off by enabling or disabling a backup from the univention gui
   }

Bareos < 16.2.0
'''''''''''''''

Older versions of Bareos handle generating the client configuration similar, but not identical:

If the name of the client is **testw1.example.com**, corresponding configuration files will be generated/adapted:

-  creates :file:`/etc/bareos/autogenerated/fd-configs/testw1.example.com.conf`

-  creates :file:`/etc/bareos/autogenerated/clients/testw1.example.com.include`

-  extends :file:`/etc/bareos/autogenerated/clients.include`

Here the files intended for the target systems are generated under :file:`/etc/bareos/autogenerated/fd-configs/` and they do not only definr a director resource, but are full configuration files for the client. After you have :ref:`installed the Bareos client on the target system <SecondClient>`, copy the generated client configuration over to the client and save it to

-  on Linux: :file:`/etc/bareos/bareos-fd.conf`

-  on Windows: :file:`C:\Program Files\Bareos\bareos-fd.conf`

.. code-block:: shell-session
   :caption: copy client configuration from the server to the testw1.example.com client (Linux)

   root@ucs:~# CLIENTNAME=testw1.example.com
   root@ucs:~# scp /etc/bareos/autogenerated/fd-configs/${CLIENTNAME}.conf root@${CLIENTNAME}:/etc/bareos/bareos-fd.conf


.. _section-WindowsInstallation:

Install on Windows
~~~~~~~~~~~~~~~~~~~~~~

Please consult the dedicated page :ref:`Windows:Installation`.


.. _section-UniversalLinuxClient:

Universal Linux Client
~~~~~~~~~~~~~~~~~~~~~~

The Bareos project provides packages for the current releases of all major Linux distributions.
In order to support even more platforms
Bareos :sinceVersion:`21.0.0: Universal Linux Client (ULC)`
provides the so called **Universal Linux Client (ULC)**.

The Universal Linux Client is a |fd|,
built in a way to have minimal dependencies to other libraries.

.. note::

   The **Universal Linux Client** depends on the **OpenSSL** library
   of the host in order to utilize security updates for this library.

It incorporates all functionality for normal backup and restore operations,
however it has only limited plugin support.

Currently it is provided as a Debian package.
However, it is planed to provide it also in other formats.

The ULC have extra repositories, their names starting with **ULC_**
(e.g. **ULC_deb_OpenSSL_1.1**)
at https://download.bareos.org/bareos/release/.
There will be different repositories depending on packaging standard
and remaining dependencies.
These repositories contain the **bareos-universal-client** package
and sometimes their corresponding debug package.
You can either add the repository to your system
or only download and install the package file.

One of ULC's goals is to support new platforms
for which native packages are not yet available.
As soon as native packages are available,
their repository can be added
and on an update the ULC package
will be seamlessly replaced by the normal |fd| package.
No change to the Bareos configuration is required.

.. warning::

   While ULC packages are designed to run on as many Linux platforms as possible,
   they should only be used
   if this platform is not directly supported by the Bareos project.
   When available, native packages should be preferred.

Feature overview:

  * Single package installation
  * Repository based installation
  * Minimal dependencies to system libraries (except OpenSSL)
  * Uses host OpenSSL library
  * Replaceable by the normal |fd|. No configuration change required.



.. _section-CreateDatabase:

Prepare Bareos database
-----------------------

We assume that you already have your PostgreSQL database server installed and basically running.

For details, see chapter :ref:`CatMaintenanceChapter`.

Debian based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since Bareos :sinceVersion:`14.2.0: dbconfig-common (Debian)` the Debian (and Ubuntu) based packages support the **dbconfig-common** mechanism to create and update the Bareos database.

Follow the instructions during install to configure it according to your needs.

.. image:: /include/images/dbconfig-1-enable.*
   :width: 80.0%


If you decide not to use **dbconfig-common** (selecting :strong:`<No>` on the initial dialog), follow the instructions for :ref:`section-CreateDatabaseOtherDistributions`.

For details see :ref:`section-dbconfig`.

.. _section-CreateDatabaseOtherDistributions:

Other Platforms
~~~~~~~~~~~~~~~

If your PostgreSQL administration user is **postgres** (default), use the following commands:

.. code-block:: shell-session
   :caption: Setup Bareos catalog with PostgreSQL

   su postgres -c /usr/lib/bareos/scripts/create_bareos_database
   su postgres -c /usr/lib/bareos/scripts/make_bareos_tables
   su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges


.. _section-StartDaemons:

Start the daemons
-----------------

.. code-block:: shell-session
   :caption: Start the Bareos Daemons

   systemctl start bareos-dir
   systemctl start bareos-sd
   systemctl start bareos-fd

Please remark, the Bareos Daemons need to have access to the TCP ports 9101-9103.

Now you should be able to log in to the director using the bconsole.

When you want to use the bareos-webui, please refer to the chapter :ref:`section-install-webui`.

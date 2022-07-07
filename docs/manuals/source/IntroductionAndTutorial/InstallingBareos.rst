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

You’ll find Bareos binary package repositories at https://download.bareos.org/. The stable releases are available at https://download.bareos.org/bareos/release/.

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

**See** https://download.bareos.org/bareos/release/ **for applicable releases and distributions.**
See :ref:`section-UniversalLinuxClient` if your distribution is not present.

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
like AlmaLinux, CentOS Stream, Oracle and Rocky Linux.


.. code-block:: sh
   :caption: Shell example script for Bareos installation on RHEL / EL / Fedora

   #!/bin/sh

   # See https://download.bareos.org/bareos/release/
   # for applicable releases and distributions

   DIST=EL_8
   # or
   # DIST=RHEL_8
   # DIST=RHEL_7
   # DIST=CentOS_7
   # DIST=Fedora_35
   # DIST=Fedora_34


   RELEASE=release/21
   # RELEASE=experimental/nightly

   # add the Bareos repository
   URL=https://download.bareos.org/bareos/$RELEASE/$DIST
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

   # See https://download.bareos.org/bareos/release/
   # for applicable releases and distributions

   DIST=SLE_15_SP3
   # or
   # DIST=SLE_15_SP2
   # DIST=SLE_12_SP5
   # DIST=openSUSE_Leap_15.3
   # DIST=openSUSE_Leap_15.2

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   # add the Bareos repository
   URL=https://download.bareos.org/bareos/$RELEASE/$DIST
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

   # See https://download.bareos.org/bareos/release/
   # for applicable releases and distributions

   DIST=Debian_11
   # or
   # DIST=Debian_10
   # DIST=Debian_9.0
   # DIST=xUbuntu_20.04
   # DIST=xUbuntu_18.04

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   URL=https://download.bareos.org/bareos/$RELEASE/$DIST

   # add the Bareos repository
   wget -O /etc/apt/sources.list.d/bareos.list $URL/bareos.list

   # add package key
   wget -q $URL/Release.key -O- | apt-key add -

   # install Bareos packages
   apt-get update
   apt-get install bareos bareos-database-postgresql

If you use the versions of Bareos directly integrated into the distributions, please note that there are some differences, see :ref:`section-DebianOrgLimitations`.


.. _section-FreeBSD:

Install on FreeBSD based Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; FreeBSD

.. code-block:: sh
   :caption: Shell example script for Bareos installation on FreeBSD

   #!/bin/sh

   # See https://download.bareos.org/bareos/release/
   # for applicable releases and distributions

   DIST=FreeBSD_13.0
   # or
   # DIST=FreeBSD_12.2

   RELEASE=release/21
   # or
   # RELEASE=experimental/nightly

   URL=https://download.bareos.org/bareos/$RELEASE/$DIST

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

Install on Univention Corporate Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Bareos offers additional functionality and integration into an Univention Corporate Server environment. Please follow the intructions in :ref:`section-UniventionCorporateServer`.

If you are not interested in this additional functionality, the commands described in :ref:`section-InstallBareosPackagesDebian` will also work for Univention Corporate Servers.



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
   :width: 45.0%

.. image:: /include/images/dbconfig-2-select-database-type.*
   :width: 45.0%




If you decide not to use **dbconfig-common** (selecting :strong:`<No>` on the initial dialog), follow the instructions for :ref:`section-CreateDatabaseOtherDistributions`.

The selectable database backends depend on the **bareos-database-*** packages installed.

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

Please remark, the Bareos Daemons need to have access to the ports 9101-9103.

Now you should be able to log in to the director using the bconsole.

When you want to use the bareos-webui, please refer to the chapter :ref:`section-install-webui`.

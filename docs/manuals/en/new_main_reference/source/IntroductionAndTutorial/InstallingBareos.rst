.. _InstallChapter:

Installing Bareos
=================

:index:`\ <single: Bareos; Installing>` :index:`\ <single: Installation; Linux>`

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

-  http://download.bareos.org/bareos/release/latest/

You’ll find Bareos binary package repositories at http://download.bareos.org/. The latest stable released version is available at http://download.bareos.org/bareos/release/latest/.

The public key to verify the repository is also in repository directory (:file:`Release.key` for Debian based distributions, :file:`repodata/repomd.xml.key` for RPM based distributions).

Section :ref:`section-InstallBareosPackages` describes how to add the software repository to your system.

.. _section-ChooseDatabaseBackend:

Decide about the Database Backend
---------------------------------

Bareos offers the following database backends:

-  PostgreSQL by package **bareos-database-postgresql**. This is the recommended backend.

-  MariaDB/MySQL by package **bareos-database-mysql**

-  | Sqlite by package **bareos-database-sqlite3**
   | 

.. warning::

   The Sqlite backend is only intended for testing, not for productive use.

PostgreSQL
   is the default backend.

MariaDB/MySQL
   backend is also included.

Sqlite
   backend is intended for testing purposes only.

The Bareos database packages have there dependencies only to the database client packages, therefore the database itself must be installed manually.

If you do not explicitly choose a database backend, your operating system installer will choose one for you. The default should be PostgreSQL, but depending on your operating system and the already installed packages, this may differ.

.. _section-InstallBareosPackages:

Install the Bareos Software Packages
------------------------------------

The package **bareos** is only a meta package, that contains dependencies to the main components of Bareos, see :ref:`section-BareosPackages`. If you want to setup a distributed environment (like one Director, separate database server, multiple Storage daemons) you have to choose the corresponding Bareos packages to install on each hosts instead of just installing the **bareos** package.

Install on RedHat based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RHEL>7, CentOS>7, Fedora
^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Platform; RHEL>` :index:`\ <single: Platform; CentOS>` :index:`\ <single: Platform; Fedora>`

Bareos :sinceVersion:`15.2.0: requires: jansson` requires the :ref:`Jansson library <jansson>` package. On RHEL 7 it is available through the RHEL Server Optional channel. On CentOS 7 and Fedora is it included on the main repository.

.. code-block:: shell-session
   :caption: Bareos installation on RHEL > 7 / CentOS > 7 / Fedora

   #
   # define parameter
   #

   DIST=RHEL_7
   # or
   # DIST=CentOS_7
   # DIST=Fedora_26
   # DIST=Fedora_25

   RELEASE=release/17.2/
   # or
   # RELEASE=release/latest/
   # RELEASE=experimental/nightly/

   # add the Bareos repository
   URL=http://download.bareos.org/bareos/$RELEASE/$DIST
   wget -O /etc/yum.repos.d/bareos.repo $URL/bareos.repo

   # install Bareos packages
   yum install bareos bareos-database-postgresql

RHEL 6, CentOS 6
^^^^^^^^^^^^^^^^

:index:`\ <single: Platform; RHEL; 6>` :index:`\ <single: Platform; CentOS; 6>`

Bareos :sinceVersion:`15.2.0: requires: jansson` requires the :ref:`Jansson library <jansson>` package. This package is available on `EPEL <https://fedoraproject.org/wiki/EPEL>`_ 6. Make sure, it is available on your system.

.. code-block:: shell-session
   :caption: Bareos installation on RHEL > 6 / CentOS > 6

   #
   # add EPEL repository, if not already present.
   # Required for the jansson package.
   #
   rpm -Uhv https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm

   #
   # define parameter
   #

   DIST=RHEL_6
   # DIST=CentOS_6

   RELEASE=release/17.2/
   # or
   # RELEASE=release/latest/
   # RELEASE=experimental/nightly/

   # add the Bareos repository
   URL=http://download.bareos.org/bareos/$RELEASE/$DIST
   wget -O /etc/yum.repos.d/bareos.repo $URL/bareos.repo

   # install Bareos packages
   yum install bareos bareos-database-postgresql

RHEL 5
^^^^^^

:index:`\ <single: Platform; RHEL; 5>`

yum in RHEL 5/CentOS 5 has slightly different behaviour as far as dependency resolving is concerned: it sometimes install a dependent package after the one that has the dependency defined. To make sure that it works, install the desired Bareos database backend package first in a separate step:

.. code-block:: shell-session
   :caption: Bareos installation on RHEL 5 / CentOS 5

   #
   # define parameter
   #

   DIST=RHEL_5

   RELEASE=release/17.2/
   # or
   # RELEASE=release/latest/
   # RELEASE=experimental/nightly/

   # add the Bareos repository
   URL=http://download.bareos.org/bareos/$RELEASE/$DIST
   wget -O /etc/yum.repos.d/bareos.repo $URL/bareos.repo

   # install Bareos packages
   yum install bareos-database-postgresql
   yum install bareos

Install on SUSE based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SUSE Linux Enterprise Server (SLES), openSUSE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:index:`\ <single: Platform; SLES>` :index:`\ <single: Platform; openSUSE>`

.. code-block:: shell-session
   :caption: Bareos installation on SLES / openSUSE

   #
   # define parameter
   #

   DIST=SLE_12_SP3
   # or
   # DIST=SLE_12_SP2
   # DIST=SLE_12_SP1
   # DIST=SLE_11_SP4
   # DIST=openSUSE_Leap_42.3
   # DIST=openSUSE_Leap_42.2

   RELEASE=release/17.2/
   # or
   # RELEASE=release/latest/
   # RELEASE=experimental/nightly/

   # add the Bareos repository
   URL=http://download.bareos.org/bareos/$RELEASE/$DIST
   zypper addrepo --refresh $URL/bareos.repo

   # install Bareos packages
   zypper install bareos bareos-database-postgresql

.. _section-InstallBareosPackagesDebian:

Install on Debian based Linux Distributions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Debian / Ubuntu
^^^^^^^^^^^^^^^

:index:`\ <single: Platform; Debian>` :index:`\ <single: Platform; Ubuntu>`

Bareos :sinceVersion:`15.2.0: requires: jansson` requires the :ref:`Jansson library <jansson>` package. On Ubuntu is it available in Ubuntu Universe. In Debian, is it included in the main repository.

.. code-block:: shell-session
   :caption: Bareos installation on Debian / Ubuntu

   #
   # define parameter
   #

   DIST=Debian_9.0
   # or
   # DIST=Debian_8.0
   # DIST=xUbuntu_16.04
   # DIST=xUbuntu_14.04
   # DIST=xUbuntu_12.04

   RELEASE=release/17.2/
   # or
   # RELEASE=release/latest/
   # RELEASE=experimental/nightly/

   URL=http://download.bareos.org/bareos/$RELEASE/$DIST

   # add the Bareos repository
   printf "deb $URL /\n" > /etc/apt/sources.list.d/bareos.list

   # add package key
   wget -q $URL/Release.key -O- | apt-key add -

   # install Bareos packages
   apt-get update
   apt-get install bareos bareos-database-postgresql

If you prefer using the versions of Bareos directly integrated into the distributions, please note that there are some differences, see :ref:`section-DebianOrgLimitations`.

Install on Univention Corporate Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:os:`Univention`

Bareos offers additional functionality and integration into an Univention Corporate Server environment. Please follow the intructions in :ref:`section-UniventionCorporateServer`.

If you are not interested in this additional functionality, the commands described in :ref:`section-InstallBareosPackagesDebian` will also work for Univention Corporate Servers.

.. _section-CreateDatabase:

Prepare Bareos database
-----------------------

We assume that you have already your database installed and basically running. Using the PostgreSQL database backend is recommended.

The easiest way to set up a database is using an system account that have passwordless local access to the database. Often this is the user **root** for MySQL and the user **postgres** for PostgreSQL.

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

PostgreSQL
^^^^^^^^^^

If your are using PostgreSQL and your PostgreSQL administration user is **postgres** (default), use following commands:

.. code-block:: shell-session
   :caption: Setup Bareos catalog with PostgreSQL

   su postgres -c /usr/lib/bareos/scripts/create_bareos_database
   su postgres -c /usr/lib/bareos/scripts/make_bareos_tables
   su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges

MySQL/MariaDB
^^^^^^^^^^^^^

Make sure, that **root** has direct access to the local MySQL server. Check if the command :command:`mysql` connects to the database without defining the password. This is the default on RedHat and SUSE distributions. On other systems (Debian, Ubuntu), create the file :file:`~/.my.cnf` with your authentication informations:

.. code-block:: cfg
   :caption: MySQL credentials file .my.cnf

   [client]
   host=localhost
   user=root
   password=<input>YourPasswordForAccessingMysqlAsRoot</input>

It is recommended, to secure the Bareos database connection with a password. See :ref:`Catalog Maintenance -- MySQL <catalog-maintenance-mysql>` about how to archieve this. For testing, using a password-less MySQL connection is probable okay. Setup the Bareos database tables by following commands:

.. code-block:: shell-session
   :caption: Setup Bareos catalog with MySQL

   /usr/lib/bareos/scripts/create_bareos_database
   /usr/lib/bareos/scripts/make_bareos_tables
   /usr/lib/bareos/scripts/grant_bareos_privileges

As some Bareos updates require a database schema update, therefore the file :file:`/root/.my.cnf` might also be useful in the future.

.. _section-StartDaemons:

Start the daemons
-----------------

.. code-block:: shell-session
   :caption: Start the Bareos Daemons

   service bareos-dir start
   service bareos-sd start
   service bareos-fd start

You will eventually have to allow access to the ports 9101-9103, used by Bareos.

Now you should be able to access the director using the bconsole.

When you want to use the bareos-webui, please refer to the chapter :ref:`section-install-webui`.  




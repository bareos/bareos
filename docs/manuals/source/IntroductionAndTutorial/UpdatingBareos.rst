
.. _UpdateChapter:

Updating and upgrading Bareos
=============================

.. index::
   pair: Bareos; Update

Bareos consists of 4 main components:

  * |dir|
  * |sd|
  * |fd|
  * |webui|


.. _section-UpdateUpgradePreamble:

Before updating or upgrading Bareos software
--------------------------------------------

.. index::
   single: Update Preamble

We consider both operations usually safe and they can be applied without restriction to all installations. The Bareos project avoids breaking changes as far as possible, so usually existing configurations will work with newer installations without changes.
Especially when updating the |dir| (together with the corresponding |sd|) it is recommended to do a number of security steps before going ahead:

- Read the :ref:`Bareos current release notes <bareos-current-releasenotes>` and watch out for changes that might impact your installation. Take special care for the :strong:`Breaking Changes` paragraph, as it contains the information about changes that require your special attention when upgrading your installation as they might require adaptions of the configuration.
- Update your operating system to the latest security and patch level of the publisher.
- Empty the running jobs queue.
- Run :strong:`BackupCatalog` or equivalent as last job (keep the most up to date database dump & configuration state).
- Stop all Bareos daemons.
- If not done before

   - Save a copy of the actual configuration used. (Usually :file:`/etc/bareos/` and :file:`/etc/bareos-webui/`.
   - Dump your database content to an easy to restore format.

- Cleanup the working dir :file:`/var/lib/bareos`

   - Remove any old crash traces :file:`*.bactrace` and :file:`*.traceback` files.
   - Remove any left debugging :file:`*.trace` files.
   - Remove any leftover :file:`*.mail` that have not been sent.
   - Remove any :file:`*core*` that are left behind.

- Rotate log files if you don't use :command:`logrotate`

   - :file:`/var/log/bareos/bareos.log`
   - :file:`/var/log/bareos/bareos-audit.log`

Now you can safely apply the update or upgrade.

.. warning::

   Please remind, that |dir| and |sd| must always have the same version.
   The version of the |fd| may differ.


.. note::

   When you change the repository and refresh it, it is likely to have a new signing key.
   You will have to accept it.


.. code-block:: shell-session
   :caption: new gpg key detection on EL 8 (dnf/yum)

      Backup Archiving Recovery Open Sourced (EL_8)                    1.6 MB/s | 1.6 kB     00:00
      Importing GPG key 0xC9FED482:
      Userid     : "Bareos 21 Signing Key <signing@bareos.com>"
      Fingerprint: 91DA 1DC3 564A E20A 76C4 CA88 E019 57D6 C9FE D482
      From       : /etc/pki/rpm-gpg/RPM-GPG-KEY-bareos-release-21
      Is this ok [y/N]: y


.. code-block:: shell-session
   :caption: new gpg key detection on SUSE (zypper)

      Forcing raw metadata refresh
      New repository or package signing key received:
      Repository:       bareos
      Key Fingerprint:  91DA 1DC3 564A E20A 76C4 CA88 E019 57D6 C9FE D482
      Key Name:         Bareos 21 Signing Key <signing@bareos.com>
      Key Algorithm:    RSA 4096
      Key Created:      Mon Dec 20 10:04:50 2021
      Key Expires:      (does not expire)
      Rpm Name:         gpg-pubkey-c9fed482-61c05542

         Note: Signing data enables the recipient to verify that no modifications occurred after the data
         were signed. Accepting data with no, wrong or unknown signature can lead to a corrupted system
         and in extreme cases even to a system compromise.

         Note: A GPG pubkey is clearly identified by its fingerprint. Do not rely on the key's name. If
         you are not sure whether the presented key is authentic, ask the repository provider or check
         their web site. Many providers maintain a web page showing the fingerprints of the GPG keys they
         are using.

      Do you want to reject the key, trust temporarily, or trust always? [r/t/a/?] (r): a



.. _section-UpdateFromCommunityToSubscription:

Updating from community to subscription binaries
------------------------------------------------

.. index::
    single: update from community to subscription
    pair: Bareos; community repository
    pair: Bareos; subscription repository

To update the installed community packages (https://download.bareos.org) to Bareos Subscription packages,
you will have to point to the subscription repositories located at https://download.bareos.com/.

Once you received your `download.bareos.com` portal/repository credentials, you can refer to the following section :ref:`section-AddSoftwareRepository` for complete instructions how-to use the :file:`add_bareos_repositories.sh` helper.

Choose the same operating system and Bareos major version you are already using.

Read the :ref:`Bareos current release notes <bareos-current-releasenotes>` to check all fixes that have been made.

Proceed to the next section, to install last minor bugfix release on your systems.


.. _section-UpdateMinorBugfix:

Updating Bareos to the lastest minor or bugfix release
------------------------------------------------------

.. index::
   single: Update latest minor bugfix
   pair: update; minor;
   pair: update; bugfix

In most cases, a Bareos update is simply done by a package update of the distribution.

.. note::

   Please before processing, apply steps in :ref:`section-UpdateUpgradePreamble`


In this section, we explain how to update your Bareos major version to the latest minor or bugfix release.

For upgrading to a new major version see :ref:`Update Bareos to a new major release <section-UpgradeMajor>`.

.. note::

   You can install directly the latest Major,Minor,Bugfix release available.
   So updating from 21.0.0 directly to 21.1.5 is not a problem.



Example how to update from 21.0.0 to 21.1.5.

.. index::
   single: update minor bugfix; RHEL
   single: update minor bugfix; CentOS
   single: update minor bugfix; Fedora
   single: update minor bugfix; EL

.. code-block:: shell-session
   :caption: Shell example command to update Bareos on on EL 8

   root@host:~# dnf upgrade --repo=bareos --refresh
      Backup Archiving Recovery Open Sourced (EL_8)            .5 kB/s | 833  B     00:00
      Dependencies resolved.
      ===================================================================================
      Package                         Architecture     Version         Repository   Size
      ===================================================================================
      Upgrading:
      bareos                          x86_64           21.1.5-3.el8    bareos      7.4 k
      bareos-bconsole                 x86_64           21.1.5-3.el8    bareos       37 k
      bareos-client                   x86_64           21.1.5-3.el8    bareos      7.5 k
      bareos-common                   x86_64           21.1.5-3.el8    bareos      764 k
      bareos-database-common          x86_64           21.1.5-3.el8    bareos       87 k
      bareos-database-postgresql      x86_64           21.1.5-3.el8    bareos       42 k
      bareos-database-tools           x86_64           21.1.5-3.el8    bareos      107 k
      bareos-director                 x86_64           21.1.5-3.el8    bareos      425 k
      bareos-filedaemon               x86_64           21.1.5-3.el8    bareos      120 k
      bareos-storage                  x86_64           21.1.5-3.el8    bareos       97 k
      bareos-tools                    x86_64           21.1.5-3.el8    bareos       52 k

      Transaction Summary
      ===================================================================================
      Upgrade  11 Packages

      Total download size: 1.7 M
      Is this ok [y/N]: y


.. index::
   single: update minor bugfix; SLE
   single: update minor bugfix; openSUSE

.. code-block:: shell-session
   :caption: Shell example command to update Bareos on SLES / openSUSE

   root@host:~# zypper refresh --force bareos
   root@host:~# zypper -v update --repo=bareos
      Verbosity: 2
      Initializing Target
      Checking whether to refresh metadata for bareos
      Retrieving: repomd.xml ..........................................[done (3.0 KiB/s)]
      Retrieving: media ......................................................[not found]
      Retrieving: repomd.xml.asc ..................................................[done]
      Retrieving: repomd.xml.key ..................................................[done]
      Retrieving: repomd.xml ......................................................[done]
      Repository:       bareos
      Key Fingerprint:  91DA 1DC3 564A E20A 76C4 CA88 E019 57D6 C9FE D482
      Key Name:         Bareos 21 Signing Key <signing@bareos.com>
      Key Algorithm:    RSA 4096
      Key Created:      Mon Dec 20 10:04:50 2021
      Key Expires:      (does not expire)
      Rpm Name:         gpg-pubkey-c9fed482-61c05542
      Retrieving: 7c2078b9b802f0f5c4edb818e870be0084ae132b4a5f21111617582fd927a65f-primary.xml.gz ...[done]
      Retrieving repository 'bareos' metadata .....................................[done]
      Building repository 'bareos' cache ..........................................[done]
      Loading repository data...
      Reading installed packages...
      Force resolution: No

      The following 10 packages are going to be upgraded:
      bareos                      21.0.0-4 -> 21.1.5-3
      bareos-bconsole             21.0.0-4 -> 21.1.5-3
      bareos-client               21.0.0-4 -> 21.1.5-3
      bareos-common               21.0.0-4 -> 21.1.5-3
      bareos-database-common      21.0.0-4 -> 21.1.5-3
      bareos-database-postgresql  21.0.0-4 -> 21.1.5-3
      bareos-database-tools       21.0.0-4 -> 21.1.5-3
      bareos-director             21.0.0-4 -> 21.1.5-3
      bareos-filedaemon           21.0.0-4 -> 21.1.5-3
      bareos-storage              21.0.0-4 -> 21.1.5-3

      10 packages to upgrade.
      Overall download size: 1.5 MiB.
      Already cached: 0 B.
      After the operation, additional 59.6 KiB will be used.
      Continue? [y/n/v/...? shows all options] (y): y

.. index::
   single: update minor bugfix; Debian
   single: update minor bugfix; Ubuntu

.. code-block:: shell-session
   :caption: Shell example command to update Bareos on Debian

   root@host:~# apt update
      Hit:1 http://deb.debian.org/debian bullseye InRelease
      Hit:2 http://deb.debian.org/debian-security bullseye-security InRelease
      Hit:3 http://deb.debian.org/debian bullseye-updates InRelease
      Get:4 https://download.bareos.com/bareos/release/21/Debian_11  InRelease [1861 B]
      Get:5 https://download.bareos.com/bareos/release/21/Debian_11  Sources [5660 B]
      Get:6 https://download.bareos.com/bareos/release/21/Debian_11  Packages [36.0 kB]
      Fetched 43.5 kB in 1s (42.3 kB/s)
      Reading package lists... Done
      Building dependency tree... Done
      Reading state information... Done
      15 packages can be upgraded. Run 'apt list --upgradable' to see them.
   root@host:~# apt upgrade
      Reading package lists... Done
      Building dependency tree... Done
      Reading state information... Done
      Calculating upgrade... Done
      The following packages will be upgraded:
      bareos bareos-bconsole bareos-client bareos-common bareos-database-common
      bareos-database-postgresql bareos-database-tools bareos-director bareos-filedaemon
      bareos-storage bareos-tools libgssapi-krb5-2 libk5crypto3 libkrb5-3
      libkrb5support0
      15 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.
      Need to get 2557 kB of archives.
      After this operation, 114 kB of additional disk space will be used.
      Do you want to continue? [Y/n] Y

.. index::
   single: update minor bugfix; FreeBSD

.. code-block:: shell-session
   :caption: Shell example command to update Bareos on FreeBSD

   root@host:~# pkg update --repository Bareos
   root@host:~# pkg upgrade --repository Bareos
      Updating Bareos repository catalogue...
      Bareos repository is up to date.
      All repositories are up to date.
      Checking for upgrades (8 candidates): 100%
      Processing candidates (8 candidates): 100%
      The following 8 package(s) will be affected (of 0 checked):
      Installed packages to be UPGRADED:
            bareos.com-bconsole: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-common: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-database-common: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-database-postgresql: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-database-tools: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-director: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-filedaemon: 21.0.0 -> 21.1.5 [Bareos]
            bareos.com-storage: 21.0.0 -> 21.1.5 [Bareos]
      Number of packages to be upgraded: 8
      1 MiB to be downloaded.
      Proceed with this action? [y/N]: y


.. _section-UpdatePostChecks:

Post update checks
~~~~~~~~~~~~~~~~~~

.. index::
   single: Update post checks

After the update, it is recommended to check if any new warnings are raised when starting the daemon, mostly deprecated configuration directives.
Bareos will mark configuration directives at least for one major release as deprecated, before removing them.

To do so you can use the `-t` flag:

.. code-block:: shell-session
   :caption: Shell example to check the Bareos configuration

   root@host:~# bareos-fd -t
   root@host:~# su - bareos -s /bin/sh -c "bareos-sd -t"
   root@host:~# su - bareos -s /bin/sh -c "bareos-dir -t"
   There are configuration warnings:
    * using deprecated keyword CollectStatistics on line 8 of file /etc/bareos/bareos-dir.d/storage/File.conf

The same warnings are also shown on a regular start of the daemons.


Depending of the operating system and its configuration, you will have to restart the daemons.
Use your operating system command to do so.


.. code-block:: shell-session
   :caption: Shell command to restart all bareos daemon with systemd on Linux

   root@host:~# systemctl restart bareos-director bareos-storage bareos-filedaemon
   root@host:~# systemctl status bareos-director bareos-storage bareos-filedaemon


.. code-block:: shell-session
   :caption: Shell command to restart all bareos daemon with service on FreeBSD

   root@host:~# service bareos-dir restart
   root@host:~# service bareos-fd restart
   root@host:~# service bareos-sd restart



.. _section-UpgradeMajor:

Upgrading Bareos to a new major release
---------------------------------------

.. index::
   single: Upgrade latest major version
   pair: Bareos; Upgrade
   pair: Upgrade; Major

In most cases, a Bareos major upgrade can be achieved by:

- Add new major repository (subscription only)
- Package upgrade of the distribution.
- Database schema upgrade with helper scripts (if schema was changed).
- Configuration review to cleanup deprecated or removed parameters.
- Review of home made scripts and manage their adaptation in case of changes.

It is generally sufficient to upgrade directly to the latest release, without having to install any intermediate releases.
However, it is required to read the release notes of all intermediate releases.

One exception is when using a |mysql| Bareos catalog,
which have been removed with Bareos :sinceVersion:`21.0.0: MySQL backend removed`.
Therefore you first have to upgrade to Bareos 20 and migrate the |mysql| into a |postgresql| Bareos Catalog, see :ref:`section-MigrationMysqlToPostgresql`.


Prepare the upgrade
~~~~~~~~~~~~~~~~~~~

If you not have already done those steps, please refer to instructions in :ref:`section-UpdateUpgradePreamble`.

.. warning::

   If you use any third party plugins, you should check and test their functionalities with the new major version beforehand.

.. _sectionUgradeMajorRepository:

Upgrade the Bareos download repositories
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This does only apply for subscription repositories (https://download.bareos.com/bareos/release/).
The community repository (https://download.bareos.org/current) will always contain the latest build of the most recent stable branch.

- First remove the existing Bareos repository definitions, by either removing the definition file(s) or by using your package manager.
- Point your browser to the new Bareos major version for your operating system on the download server.
- Open or save the helper script :file:`add_bareos_repositories.sh`.

  - You can refer to the following section :ref:`section-AddSoftwareRepository` for complete instructions how to use the :file:`add_bareos_repositories.sh` helper.

- Transfer the file to your Bareos server, and execute it as **root**.
  This will create (or depending on your OS update) the Bareos repository information.

.. code-block:: shell-session
   :caption: Shell command to upgrade the Bareos repository

   root@host:~# sh add_bareos_repositories.sh

You should be able now to proceed the appropriate commands to refresh the packages list and upgrade the package to the newer version.

.. note::

   You can refer to section :ref:`section-UpdateMinorBugfix` for commands example.


.. _sectionUpdateConfigurationFiles:

Updating the configuration files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When updating Bareos through the distribution packaging mechanism,
the existing configuration files are kept as they are.

However, configuration files installed by Bareos packages
that have been manually removed by the user will get reinstalled by the package,
see :ref:`section-ConfigurationResourceFileConventions`.


If you don’t want to modify the behavior, there is normally no need to modify the configuration.

However, in some rare cases, configuration changes are required. These cases are described in the :ref:`Release Notes <releasenotes>`.


With Bareos version >= 16.2.4 the default configuration uses the :ref:`section-SubdirectoryConfigurationScheme`.
This scheme offers various improvements. However, if your are updating from earlier versions, your existing single configuration files (:file:`/etc/bareos/bareos-*.conf`) stay in place and are contentiously used by Bareos.
The new default configuration resource files will also be installed (:file:`/etc/bareos/bareos-*.d/*/*.conf`).
However, they will only be used, when the legacy configuration file does not exist.

See :ref:`section-UpdateToConfigurationSubdirectories` for details and how to migrate to :ref:`section-SubdirectoryConfigurationScheme`.


.. _sectionUpdateDatabaseScheme:

Updating the database scheme
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes improvements in Bareos make it necessary to update the database scheme.


.. warning::

   If the Bareos catalog database does not have the current schema, the Bareos Director refuses to start.


.. code-block:: shell-session
   :caption: Shell example of bareos-dir failing to start due to lack of database schema update

   root@host:~# su - bareos -s /bin/sh -c "bareos-dir -t"
   bareos-dir: dird/check_catalog.cc:64-0 Could not open Catalog "MyCatalog", database "bareos".
   bareos-dir: dird/check_catalog.cc:71-0 Version error for database "bareos". Wanted 2210, got 2192
   bareos-dir ERROR TERMINATION
   Please correct the configuration in /etc/bareos/bareos-dir.d/*/*.conf


Detailed information can then be found in the log file :file:`/var/log/bareos/bareos.log`.

Take a look into the :ref:`Release Notes <releasenotes>` to see which Bareos updates do require a database scheme update.


.. warning::

   Especially the upgrade to Bareos >= 17.2.0 restructures the **File** database table.
   In larger installations this is very time consuming (up to several hours or days)
   and temporarily doubles the amount of required database disk space.


.. _section-UpdateDatabaseDebianDistributions:

Debian based Linux Distributions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since Bareos :sinceVersion:`14.2.0: dbconfig-common (Debian)` the Debian (and Ubuntu) based packages support the **dbconfig-common** mechanism to create and update the Bareos database. If this is properly configured, the database schema will be automatically adapted by the Bareos packages.

For details see :ref:`section-dbconfig`.

If you disabled the usage of **dbconfig-common**, follow the instructions for :ref:`section-UpdateDatabaseOtherPlatforms`.


.. _section-UpdateDatabaseOtherPlatforms:

Other Platforms
^^^^^^^^^^^^^^^

This has to be done as database administrator.
On most platforms Bareos knows only about the credentials to access the Bareos database, but not about the database administrator credentials to modify the database schema.

The task of updating the database schema is done by the scripts :command:`/usr/lib/bareos/scripts/update_bareos_tables` and :command:`/usr/lib/bareos/scripts/grant_bareos_privileges`.

However, this script requires administration access to the database. Depending on your distribution, this requires different preparations.

More details can be found in chapter :ref:`Catalog Maintenance <CatMaintenanceChapter>`.

.. code-block:: shell-session
   :caption: Update PostgreSQL database schema on most Linux distribution

   su postgres -c /usr/lib/bareos/scripts/update_bareos_tables
   su postgres -c /usr/lib/bareos/scripts/grant_bareos_privileges

The :command:`grant_bareos_privileges` command is required, if new databases tables are introduced. It does not hurt to run it multiple times.

After this, restart the Bareos Director and verify it starts without problems.

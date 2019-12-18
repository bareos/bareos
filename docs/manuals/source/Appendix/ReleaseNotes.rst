.. _releasenotes:

Release Notes
=============

.. index:: Releases

.. warning::

      While all the source code is published on `GitHub <https://github.com/bareos/bareos>`_, the releases of packages on http://download.bareos.org is limited to the initial versions of a major release. Later maintenance releases are only published on https://download.bareos.com.

This chapter concentrates on things to do when updating an existing Bareos installation.

Please note, that some of the subreleases are only development releases.

The feature overview for a release are shown at the :ref:`genindex` of this document.


.. _bareos-current-releasenotes:

.. _bareos-1924-releasenotes:

.. _bareos-19.2.4:

Bareos-19.2
-----------

Bareos-19.2.4~rc1
~~~~~~~~~~~~~~~~~

General Information
^^^^^^^^^^^^^^^^^^^

.. list-table:: Bareos 19.2.4 Release Information
   :header-rows: 0
   :widths: auto

   * - **Release Date**
     - 1 December 2019
   * - **Database Version**
     -  2192
   * - **URL**
     - https://download.bareos.com/bareos/release/19.2/
   * -
     - https://download.bareos.org/bareos/release/19.2/
..
   * - **Release Ticket**
     - :mantis:`0000`

.. csv-table:: binary package availablility in the `bareos.com subscription repos <https://www.bareos.com/en/Subscription.html>`_
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   FreeBSD_11.2, "x86_64"
   MacOS, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   SLE_12_SP3, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   Univention_4.3, "x86_64"
   Windows, "32Bit, 64Bit"
   xUbuntu_14.04, "i586,x86_64"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

New Features
^^^^^^^^^^^^
* |webui|: :ref:`section-webui-restore` a specific file version
* Display a summary before performing a restore in the |webui|
* New configuration parameter *filetree_refresh_timeout* has been added to configuration.ini file of the |webui| to set a custom timeout while refreshing the file tree (.bvfs_cache_update) in the restore module (:ref:`section-updating-bvfs-cache-frequently`)
* Configuration: Add :ref:`DirectorResourceUser` for pam authentication
* Configuration: : Simplified :ref:`StorageResourceMultipliedDevice` Storage Configuration
* Client initiated connection: Run dedicated jobs when a client connects to the |dir|. Introduced a new configuration directive RunOnIncomingConnectInterval, see the documentation here: :config:option:`dir/job/RunOnIncomingConnectInterval`
* Python-bareos: Depending on the distribution, the Python module is packaged for Python 2 and/or Python 3. Previously it has only been packaged for Python 2
* Python-bareos: There are two variants of the Console protocol. The protocol used before Bareos-18.2 and the protocol used thereafter. The protocol since Bareos-18.2 supports TLS-PSK and PAM authentication, see :ref:`bareos-18.2.5`. Beginning with this version, Python-bareos also supports both protocols. As TLS-PSK for Python (module **sslpsk**) is not available for all platforms, Python-bareos has integrated an automatic fallback to the old protocol.

Changed Features
^^^^^^^^^^^^^^^^
* Improved Command ACL handling and error messages in the |webui|
* Restricted-console profile examples for limited and read-only purposes have been added to the |webui| package
* Updated |webui| localization
* Configuration: Do not allow empty passwords for mandatory passwords
* All daemons: Generate a log message if extended attributes are disabled automatically
* All daemons: replace crc32 implementation with a faster one
* |dir|: Add support for MySQL 8
* PAM authentication requires a *User* configuration resource. Previously *Console* and *User* resources could be used.
* The *User* configuration resource changed.  Previously it has been an alias to the *Console* resource, which requires a password and allows some additional options. Now it only consists of a Name and ACL definitions, see :ref:`DirectorResourceUser`.
* Database schema update from version 2171 to 2192

Deprecated and Removed Features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* |webui|: Functionality of pre-selecting a backup client in the restore module has been removed

Bugs Fixed
^^^^^^^^^^
* :mantis:`385`: Bareos daemon stop or restart hangs if bareos-tray-monitor is connected
* :mantis:`975`: .bvfs\_lsdirs limit offset command parameters do not work properly
* :mantis:`990`: SD sometimes tries to load empty tape slot
* :mantis:`1004`: Very high cpu usage on Debian stretch
* :mantis:`1030`: Bscan does not work for migration and copy jobs
* :mantis:`1034`: Read-error on tape may be misinterpreted as end-of-tape
* :mantis:`1045`: Webui login problem
* :mantis:`1049`: Translation in German webUI
* :mantis:`1056`: NDMP restore on 16.2.5 and above does not fill NDMP environment correctly
* :mantis:`1058`: 18.2.6 build error because cmake does not build the correct dependencies
* :mantis:`1059`: Webui spams Apache error_log with bconsole messages
* :mantis:`1064`: Bconsole crashes after 5 minutes in restore operation due to forgotten watchdog
* :mantis:`1072`: Newer versions of Fedora use stricter code checking
* :mantis:`1073`: Pthread\_detach for FreeBSD (PR169)
* :mantis:`1091`: NDMP to NDMP Copy Job Fails
* :mantis:`1095`: |webui|: When login as a user without the permission to the ".api" command, the webui shows a wrong and ugly error message
* :mantis:`1100`: Bconsole crashes when pam authentication aborts
* :mantis:`1112`: After mount/unmount of tape "status slots" shows empty list
* :mantis:`1123`: Director can crash during TwoWay Authentication
* :mantis:`1149`: Audit messages are not logged any more
* :mantis:`1150`: dbconfig schema update scripts broken since 18.2
* :mantis:`1161`: Tremendous MySQL load
* :mantis:`1188`: Integer out of range when using large amounts of files with Base Jobs
* All daemons: Fix buffer overrun in PathAppend
* |dir|: Add support for MySQL 8
* |dir|: Fix nullptr cornercase in mtx-changer parser
* |webui|: Fix overflowing top navigation bar content hiding tab navigation in some modules
* |webui|: Fix always active debug messages in error.log
* |webui|: Bvfs cache update notification added
* |webui|: Fix Application Controller Plugin CommandACLPlugin


Updated Documentation
^^^^^^^^^^^^^^^^^^^^^
* :ref:`section-updating-bvfs-cache-frequently`
* |webui| Command ACL Requirements: :ref:`section-webui-command-acl-requirements`
* |webui| Access Control Configuration: :ref:`section-webui-access-control-configuration`
* |webui| Restore: :ref:`section-webui-restore`
* Developer Guide: :ref:`section-dev-webui-command-usage-in-modules`
* Documentation: Add message diagrams for backup, restore and verify
* Documentation: Correct configuration expamles and rewrite several feature introductions
* Documentation: Improve documentation of postgresql database schema and add diagrams

Internal Project Changes
^^^^^^^^^^^^^^^^^^^^^^^^
* All daemons: Smartalloc has been removed from the sourcecode
* All daemons: Removed many compiler warnings
* All daemons: Refactored scheduler, threadlist, configuration parser and recently used job-list code to be more robust and testable
* Documentation: Merge new documentation-source structure for Sphinx-build
* Removed PHP Warnings and Notices, JS and CSS errors


.. _bareos-1827-releasenotes:

.. _bareos-18.2.7:

Bareos-18.2
-----------

Bareos-18.2.7
~~~~~~~~~~~~~

General Information
^^^^^^^^^^^^^^^^^^^

.. list-table:: Bareos 18.2.7 Release Information
   :header-rows: 0
   :widths: auto

   * - **Release Date**
     - 12 December 2019
   * - **Database Version**
     -  2171
   * - **URL**
     - https://download.bareos.com/bareos/release/18.2/

   * - **Release Ticket**
     - :mantis:`1152`

.. csv-table:: binary package availablility in the `bareos.com subscription repos <https://www.bareos.com/en/Subscription.html>`_
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   CentOS_8, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Debian_10, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   Fedora_30, "x86_64"
   Fedora_31, "x86_64"
   FreeBSD_11.3, "x86_64"
   FreeBSD_12.0, "x86_64"
   FreeBSD_12.1, "x86_64"
   MacOS, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   RHEL_8, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   SLE_15_SP1, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   openSUSE_Leap_15.1, "x86_64"
   Univention_4.3, "x86_64"
   Windows, "32Bit, 64Bit"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

Bugfixes and Changes
^^^^^^^^^^^^^^^^^^^^
* :mantis:`990`: SD sometimes tries to load empty tape slot
* :mantis:`1030`: Bscan does not work for migration and copy jobs
* :mantis:`1056`: NDMP restore on 16.2.5 and above does not fill NDMP environment correctly
* :mantis:`1058`: 18.2.6 build error while cmake don't build the correct dependency's
* :mantis:`1059`: Webui spams Apache error_log with bconsole messages
* :mantis:`1072`: Newer versions of Fedora use stricter code checking
* :mantis:`1095`: |webui|: When login as a user without the permission to the ".api" command, the webui show a wrong and ugly error message
* :mantis:`1100`: Bconsole crashes when a pam authentication aborts
* :mantis:`1112`: After mount/unmount of tape "status slots" shows empty list
* :mantis:`1149`: Audit messages are not logged any more
* :mantis:`1150`: dbconfig schema update scripts broken since 18.2
* All daemons: Fix buffer overrun in PathAppend
* |dir|: Add support for MySQL 8
* |dir|: Fix nullptr cornercase in mtx-changer parser
* |dir|: Fix audit messages
* |webui|: Fix overflowing top navigation bar content hiding tab navigation in some modules
* |webui|: Fix always active debug messages in error.log
* |webui|: Bvfs cache update notification added
* Documentation: Various improvements and updates

.. _bareos-1826-releasenotes:

.. _bareos-18.2.6:

Bareos-18.2.6
~~~~~~~~~~~~~

General Information
^^^^^^^^^^^^^^^^^^^

.. list-table:: Bareos 18.2.6 Release Information
   :header-rows: 0
   :widths: auto

   * - **Release Date**
     - 13 February 2019
   * - **Database Version**
     -  2171
   * - **URL**
     - https://download.bareos.com/bareos/release/18.2/
   * - **Release Ticket**
     - n/a

.. csv-table:: binary package availablility in the `bareos.com subscription repos <https://www.bareos.com/en/Subscription.html>`_
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   FreeBSD_11.2, "x86_64"
   MacOS, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   SLE_12_SP3, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   Univention_4.3, "x86_64"
   Windows, "32Bit, 64Bit"
   xUbuntu_14.04, "i586,x86_64"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

New Features
^^^^^^^^^^^^
* New packages for MacOS and FreeBSD
* Updated documentation
* :mantis:`1045`: Fixed TLS-Cert problem with old PHP versions in the |webui|
* dbcheck: Completed merge of "Fix dbcheck orphaned path entries performance issue" (a8f2a39)


.. _bareos-1825-releasenotes:

.. _bareos-18.2.5:

Bareos 18.2.5
~~~~~~~~~~~~~

General Information
^^^^^^^^^^^^^^^^^^^

.. list-table:: Bareos 18.2.5 Release Information
   :header-rows: 0
   :widths: auto

   * - **Release Date**
     - 31 January 2019
   * - **Database Version**
     -  2171
   * - **URL**
     - http://download.bareos.org/bareos/release/18.2/

   * - **Release Ticket**
     - :mantis:`1040`

.. csv-table:: binary package availablility
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   SLE_12_SP3, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   Univention_4.3, "x86_64"
   Windows, "32Bit, 64Bit"
   xUbuntu_14.04, "i586,x86_64"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

New Features
^^^^^^^^^^^^


* New network Protocol using immediately TLS

  * TLS is immediately used on all network connections
  * Support for TLS-PSK in all daemons
  * Automatic encryption of all network traffic with TLS-PSK
  * Full Compatibility with old |bareosFd|

    * Old |bareosFd| speaking the old protocol are automatically detected
      and daemons switch to the old protocol

  * Easily update without configuration changes

    * Existing Bareos installations can be upgraded without configuration changes

* PAM Support

  * Detailed information follows
  * Introduction of new "User" Resource
  * The |bareosDir| supports PAM for user authentication
  * The Bareos WebUI supports PAM user authentication against the |bareosDir|

Changed Features
^^^^^^^^^^^^^^^^
* Bandwidth limiting now also works in TLS encrypted connections. Before, bandwidth limiting
  was ignored when the connections were TLS encrypted

* Droplet (S3): multiple enhancements

* |bconsole|: Added "whoami" command to show currently associated user

* xattr and acl support now are enabled by default

  * Both features were disabled by default and needed to be enabled in the fileset options
  * Now both are enabled by default and can be disabled in the fileset options
  * New |bareosFd| logs the current status of both options in job log

Backward compatibility
^^^^^^^^^^^^^^^^^^^^^^
* |bareosDir| >= 18.2 can work with all |bareosFd| versions. However, all other components need to be updated to Bareos version >= 18.2
* To maintain |bareosWebui| access to the |bareosDir|, it depends on the current configuration. 1. TLS certificates: Nothing to do. 2. No TLS configured: Set TlsEnable=false in the respective console config of the |bareosWebui| in the |bareosDir|

..  * |bconsole| < 18.2 can be used with minor drawbacks (no PAM authentication, no TLS-PSK)

Full connection overview
^^^^^^^^^^^^^^^^^^^^^^^^
This diagram contains all possible connections between Bareos components
that are virtually usable. The numbers in each component are the version
numbers of this component that can be used with a Bareos 18.2 system
(Director Daemon and Storage Daemon). However, to be able to use all feature
it is recommended to use all components from version 18.2.

For a detailed explanation of all connection modes see :ref:`ConnectionOverviewReference`.

.. uml::
  :caption: Full overview of all Bareos connections possible with Bareos 18.2

  left to right direction
  skinparam shadowing false

  (Python 17,18) as Py1718
  (Console 17,18) as Con1718
  (WebUI 17,18) as Webui1718
  (Tray Monitor 18) as Tray18

  [Filedaemon 17,18] as FD1718
  [Directordaemon 18] as Dir18
  [Storagedaemon 18] as SD18
  [Storagedaemon2 18] as SD218

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow(Con1718, right, Dir18, 1n)
  arrow(Con1718, right, Dir18, 2r)

  arrow(Py1718, up, Dir18, 3n)
  arrow(Py1718, up, Dir18, 4r)

  arrow(Webui1718, down, Dir18, 5n)
  arrow(Webui1718, down, Dir18, 6r)

  arrow(Dir18, up, FD1718, 7)
  arrow(FD1718, down, Dir18, 8)

  arrow(Dir18, right, SD18, 9a)

  arrow(FD1718, down, SD18, 10)
  arrow(SD18, down, FD1718, 11)

  arrow(SD18, down, SD218, 12)
  arrow(Dir18, down, SD218, 9b)

  arrow(Tray18, down, Dir18, 13)
  arrow(Tray18, down, FD1718, 14)
  arrow(Tray18, down, SD18, 15)

Deprecated and Removed Features
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* Removed Bareos conio option, as the standard library readline is used instead
* GnutTLS is not supported anymore, OpenSSL is now required


Bugs Fixed
^^^^^^^^^^
* :mantis:`845`: NetApp OnCommand System Manager calls on SD Port 10000 leads to Segmentation Violation
* :mantis:`805`: Can't restore vmware-plugin assisted backups via |bareosWebui|
* Windows Installer: Fixed infinite install dialog for VC 2012 checks on x86 windows
* Fixed memory leaks in the |bareosDir| when using bconsole or |bareosWebui|
* Fixed a debug message handler bug on |bareosDir| when debuglevel is >= 900
* Improved shutdown of |bareosDir|
* :mantis:`1034`: Read error on tape may be misinterpreted as end-of-tape
* "Exit On Fatal" works now as expected
* Fixed a bug when migration storage daemons cannot connect
* Guarded numerous nullpointers
* VMware: Fixed errors when using non-ascii characters

Updated Documentation
^^^^^^^^^^^^^^^^^^^^^
* Updated VMware plugin documentation: :ref:`VMwarePlugin`
* How to configure transport encryption in |bareosWebui|: :ref:`TransportEncryptionWebuiBareosDirChapter`
* Detailed connections overview here: :ref:`ConnectionOverviewReference`
* How to use PAM with |bareosDir|: :ref:`PAMConfigurationChapter`
* Backward compatibility of |bareosFd|: :ref:`CompatibilityWithFileDaemonsBefore182Chapter`

Internal Project Changes
^^^^^^^^^^^^^^^^^^^^^^^^
* Reorganized the whole git repository and merged sub repositories into main repository
* Changed the build system from autoconf/automake to cmake
* Switched from cmocka to google test framework for unit tests
* Introduced namespaces to avoid name clashes when parts of different daemons are tested in one test
* Switched to use c++11 standard, start to refactor using standard library instead of legacy features
* Use google c++ style guide

  * Refactored variable names

* Refactored configuration parser
* TLS implementation has now a base class interface instead of compile time switched behaviour
* Library cleanup and reorganization

  * Library does not use main program variables anymore
  * Removed libbareoscfg
  * Enhanced windows cross building

* Renamed c++ files to use .cc file extension
* Cleanup of header files

  * Removed "protos.h"
  * Introduced individual header fileS for each c++ file
  * Each header file has own google c++ standard header guard
  * Explicitly declare functions override where applicable


* |bareosTraymonitor|: Allows compiling using Qt4 or Qt5
* Switch the documentation from LaTeX to Sphinx (work in progress)
* |bareosWebui|: Enhances Selenium tests to be used on https://saucelabs.com/u/bareossaucelabs
* clang: Massively reduced number of warnings
* FreeBSD: added start scripts, fixed buggy cmake detection of ACL support
* Regression tests

  * Automatically build |bareosTraymonitor|
  * Preconfigure |bareosWebui| to run in php's own webserver for easy testing





Bareos-17.2
-----------

.. _bareos-17.2.8:

bareos-17.2.8
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.8; Release Notes>`\

================ ===============================================
Code Release     2019-12-12
Database Version 2171 (unchanged)
Release Ticket   :mantis:`1153`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

-  Droplet: improves handling when truncating volumes

   -  Without this change, errors when truncating a droplet volume are silently ignored.

-  :mantis:`1030`  bscan does not work for migration and copy jobs

-  :mantis:`1034`: Read error on tape may be misinterpreted as end-of-tape.

-  Fix nullptr cornercase in mtx-changer parser in the |dir|

-  Fix corner-case crash during job cancel in the |sd|

-  Fix crash on excessive SOS records

-  Packaging: make specfiles compatible to docker


.. _bareos-17.2.7:

bareos-17.2.7
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.7; Release Notes>`\

================ ===============================================
Code Release     2018-07-13
Database Version 2171 (unchanged)
Release Ticket   :mantis:`966`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  :mantis:`892` **bareos-storage-droplet**: improve error handling on unavailable backend.

-  :mantis:`902` **bareos-storage-droplet**: improve job status handling (terminate job after all data is written).

-  :mantis:`967` :os:`Windows`: overwrite symbolic links on restore.

-  :mantis:`983` |sd|: prevent sporadic crash when :config:option:`sd/storage/CollectJobStatistics = yes`\ .

-  :os:`SLES 12sp2` and :os:`SLES 12sp3`: provide **bareos-storage-ceph** and **bareos-filedaemon-ceph-plugin** packages.

.. _bareos-17.2.6:

bareos-17.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.6; Release Notes>`\ 

================ ===============================================
Code Release     2018-06-21
Database Version 2171 (unchanged)
Release Ticket   :mantis:`916`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  added platforms: :os:`Fedora 27`, :os:`Fedora 28`, :os:`openSUSE 15.0`, :os:`Ubuntu 18.04` and :os:`Univention 4.3`.

-  :os:`Univention 4.3`: fixes integration.

-  :mantis:`872` adapted to new Ceph API.

-  :mantis:`943` use **tirpc** if Sun-RPC is not provided.

-  :mantis:`964` fixes the predefined queries.

-  :mantis:`969` fixes a problem of restoring more files then selected in |webui|/BVFS.

-  |dir|: fixes for a crash after reload in the statistics thread (:mantis:`695`, :mantis:`903`).

-  :command:`bareos-dbcheck`: cleanup and speedup for some some of the checks.

-  adapted for |postgresql| 10.

-  gfapi: stale file handles are treated as warnings

.. _bareos-17.2.5:

bareos-17.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.5; Release Notes>`\

================ ===============================================
Code Release     2018-02-16
Database Version 2171 (unchanged)
Release Ticket   :mantis:`910`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  |fd| is ready for :os:`AIX 7.1.0.0`.

-  :ref:`VMwarePlugin` is also provided for :os:`Debian 9`.

-  NDMP fixes

-  Virtual Backup fixes

-  **bareos-storage-droplet**: improvements

-  :command:`bareos-dbcheck` improvements and fixes: with older versions it could happen, that it destroys structures required by :bcommand:`.bvfs_*`.

-  :mantis:`850` fixes a bug on :os:`Univention`: fixes a problem of regenerating passwords when resyncing settings.

-  :mantis:`890` :bcommand:`.bvfs_update` fix. Before there have been cases where it did not update the cache.

-  :bcommand:`.bvfs_lsdirs` make limit- and offset-option work correctly.

-  :bcommand:`.bvfs_lsdirs` show special directory (like :file:`@bpipe@/`) on the same level as :file:`/`.

-  :mantis:`895` added description to the output of :bcommand:`show filesets`.

-  |webui|: Restore Browser fixes

   -  There was the possibility of an endless loop if the BVFS API delivers unexpected results. This has been fixed. See bugreports :mantis:`887` and :mantis:`893` for details.

   -  :mantis:`905` fixes a problem with file names containing quotes.

-  :config:option:`dir/client/NdmpBlockSize`\  changed type from :strong:`Pint32` to :strong:`Size32`. This should not affect any configuration, but is more consistent with other block size configuration directives.

.. _bareos-17.2.4:

bareos-17.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.4; Release Notes>`\

================ ===============================================
Code Release     2017-12-14
Database Version 2171
Release Ticket   :mantis:`861`
Url              http://download.bareos.org/bareos/release/17.2/
\                http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several enhancements. Excerpt:

-  Bareos Distribution (packages)

   -  **Python-bareos** is included in the core distribution.

   -  **bareos-storage-droplet** is a storage backend for the droplet library. Most notably it allows backup and restores to a S3 environment. \betaSince{sd}{bareos-storage-droplet}{17.2.4}

   -  **bat** has been removed.

   -  platforms:

      -  Windows Clients are still supported since Windows Vista.

      -  MacOS: added to build chain.

      -  |fd| is ready for HP-UX 11.31 (ia64).

      -  Linux Distribution: Bareos tries to provide packages for all current platforms. For details, refer to :ref:`section-packages`.

   -  Linux RPM packages: allow read access to /etc/bareos/ for all users (however, relevant files are still only readable for the user **bareos**). This allows other programs associated with Bareos to also use this directory.

-  Denormalization of the **File** database table

   -  The denormalization of the **File** database table leads to enormous performance improvements in installation, which covering a lot of file (millions and more).

   -  For the denormalization the database schema must be modified. 

.. warning::

   Updating the database to schema version >= 2170 will increase the required disk space.
                      Especially it will require around twice the amount of the current database disk space during the migration.

   -  The **Filename** database table does no longer exists. Therefore the :bcommand:`.bvfs_*` commands do no longer output the **FilenameId** column.

-  NDMP_NATIVE support has been added. This include the NDMP features DAR and DDAR. For details see :ref:`section-NdmpNative`.

-  Updated the package **bareos-vmware-plugin** to utilize the Virtual Disk Development Kit (VDDK) 6.5.x. This includes support for |vsphere| 6.5 and the next major release (except new features) and backward compatible with |vsphere| 5.5 and 6.0. For details see :ref:`VMwarePlugin`.

-  Soft Quota: automatic quota grace period reset if a job does not exceed the quota.

-  :command:`bareos-dbcheck`: disable all interactive questions in batch mode.

-  :bcommand:`list files`: also show deleted files (accurate mode).

-  :bcommand:`list jobstatastics`: added.

-  :bcommand:`purge`: added confirmation.

-  :bcommand:`list volumes`: fix limit and offset handling.

-  :mantis:`629` Windows: restore directory attributes.

-  :mantis:`639` tape: fix block size handling, AWS VTL iSCSI devices

-  :mantis:`705` support for MySQL 5.7

-  :mantis:`719` allow long JSON messages (has been increased from 100KB to 2GB).

-  :mantis:`793` Virtual Backups: skip jobs with no files.

Bareos-16.2
-----------

.. _bareos-16.2.9:

bareos-16.2.9
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.9; Release Notes>`\

================ ===============================================
Code Release     2019-12-12
Database Version 2004 (unchanged)
Release Ticket   :mantis:`1154`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

-  Improve list command

   -  Honor "pool" filter for jobs so you can list jobs by pool

-  Updated MySQL creation schema to current standards

-  Packaging: Use .tar.bz2 instead of tar.gz

-  Packaging: Make rpm spec compatible to docker builds


.. _bareos-16.2.8:

bareos-16.2.8
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.8; Release Notes>`\

================ ===============================================
Code Release     2018-07-06
Database Version 2004 (unchanged)
Release Ticket   :mantis:`863`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  gfapi-fd Plugin

   -  Allow to use non-accurate backups with glusterfind

   -  Fix backups with empty glusterfind filelist.

   -  Explicitly close glfs fd on IO-open

   -  Don’t reinitialize the connection to gluster

   -  Fix parsing of missing basedir argument

   -  Handle non-fatal Gluster problems properly

-  Reset JobStatus to previous JobStatus in status SD and FD loops to fix status all output

-  Backport ceph: ported cephfs-fd and :command:`cephfs_device` to new api

-  :mantis:`967` Windows: Symbolic links are now replaceable during restore

.. _bareos-16.2.7:

bareos-16.2.7
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.7; Release Notes>`\ 

================ ===============================================
Code Release     2017-10-09
Database Version 2004 (unchanged)
Release Ticket   :mantis:`836`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  Fixes a Director crash, when enabling debugging output

-  :bcommand:`.bvfs_lsdirs`: improve performance, especially when having a large number of directories

   -  To optimize the performance of the SQL query used by :bcommand:`.bvfs_lsdirs`, it is important to have the following indexes:

   -  PostgreSQL

      -  ``CREATE INDEX file_jpfnidpart_idx ON File(PathId,JobId,FilenameId) WHERE FileIndex = 0;``

      -  If the index ``file_jfnidpart_idx`` mentioned in 16.2.6 release notes exist, drop it:
         ``DROP INDEX file_jfnidpart_idx;``

   -  MySQL/MariaDB

      -  ``CREATE INDEX PathId_JobId_FileNameId_FileIndex ON File(PathId,JobId,FilenameId,FileIndex);``

      -  If the index ``PathId_JobId_FileIndex_FileNameId`` mentioned in 16.2.6 release notes exist, drop it:
         ``DROP INDEX PathId_JobId_FileIndex_FileNameId ON File;``

-  Utilize OpenSSL >= 1.1 if available

-  Windows: fixes silent upgrade (:command:`winbareos-*.exe /S`)

-  Windows: restore attributes also on directories (not only on files)

-  Fixes problem with SHA1 signature when compiled without OpenSSL (not relevant for bareos.org/bareos.com packages)

-  Packages for openSUSE Leap 42.3 and Fedora 26 have been added.

-  Packages for AIX and current HP-UX 11.31

.. _bareos-16.2.6:

bareos-16.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.6; Release Notes>`\ 

================ ===============================================
Code Release     2017-06-22
Database Version 2004 (unchanged)
Release Ticket   :mantis:`794`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  Prevent from director crash when using incorrect paramaters of :bcommand:`.bvfs_*` commands.

-  Director now closes all configuration files when reloading failed.

-  Storage daemon now closes the network connection when MaximumConcurrentJobs reached.

-  New directive :strong:`LanAddress`\  was added to the Client and Storage Resources of the director to facilitate a network topology where client and storage are situated inside of a LAN, but the Director is outside of that LAN. See :ref:`LanAddress` for details.

-  A Problem in the storage abstraction layer was fixed where the director picked the wrong storage daemon when multiple storages/storage daemons were used.

-  The device spool size calculation when using secure erase was fixed.

-  :bcommand:`.bvfs_lsdirs` no longer shows empty directories from accurate jobs.

   -  

      

         .. warning::

            This decreases performance if your environment has a large numbers of directories. Creating an index improves the performance.

   -  

      |postgresql|

      -  When using PostgreSQL, creating the following partial improves the performance sufficiently:
         ``CREATE INDEX file_jfnidpart_idx ON File(JobId, FilenameId) WHERE FileIndex = 0;``

      -  Run following command to create the partial index:
         :file:`su - postgres -c 'echo "CREATE INDEX file_jfnidpart_idx ON File(JobId, FilenameId) WHERE FileIndex = 0; ANALYZE File;" | psql bareos'`

   -  

      |mysql|

      -  When using MySQL or MariaDB, creating the following index improves the performance:
         ``CREATE INDEX PathId_JobId_FileIndex_FileNameId ON File(PathId,JobId,FileIndex,FilenameId);``

      -  Run following command to create the index:
         :file:`echo "CREATE INDEX PathId_JobId_FileIndex_FileNameId ON File(PathId,JobId,FileIndex,FilenameId);" | mysql -u root bareos`

      -  However, with larger amounts of directories and/or involved jobs, even with this index the performance of :bcommand:`.bvfs_lsdirs` may still be insufficient. We are working on optimizing the SQL query for MySQL/MariaDB to solve this problem.

-  Packages for Univention UCS 4.2 have been added.

-  Packages for Debian 9 (Stretch) have been added.

-  WebUI: The post install script of the bareos-webui RPM package for RHEL/CentOS was fixed, it no longer tries to run a2enmod which does not exist on RHEL/CentOS.

-  WebUI: The login form no longer allows redirects to arbitrary URLs

-  WebUI: The used ZendFramework components were updated from version 2.4.10 to 2.4.11.

-  WebUI: jQuery was updated from version 1.12.4 to version 3.2.0., some outdated browsers like Internet Explorer 6-8, Opera 12.1x or Safari 5.1+ will no longer be supported, see `jQuery Browser Support <http://jquery.com/browser-support/>`_ for details.

.. _bareos-16.2.5:

bareos-16.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.5; Release Notes>`\ 

================ ===============================================
Code Release     2017-03-03
Database Version 2004 (unchanged)
Release Ticket   :mantis:`734`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  NDMP: critical bugfix when restoring large files.

-  truncate command allows to free space on disk storages (replaces an purged volume by an empty volume).

-  Some fixes were added regarding director crashes, Windows backups (VSS), soft-quota reset and API (bvfs) problems.

-  WebUI: handle file names containing special characters, hostnames starting with numbers and long logfiles.

-  WebUI: adds translations for Chinese, Italian and Spanish.

.. _bareos-16.2.4:

bareos-16.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.4; Release Notes>`\ 

================ ===============================================
Code Release     2016-10-28
Database Version 2004 (unchanged)
Release Ticket   :mantis:`698`
Url              http://download.bareos.org/bareos/release/16.2/
\                http://download.bareos.com/bareos/release/16.2/
================ ===============================================

First stable release of the Bareos 16.2 branch.

-  Configuration

   -  Bareos packages contain the default configuration in :ref:`section-ConfigurationSubdirectories`. Please read :ref:`section-UpdateToConfigurationSubdirectories` before updating (make a copy of your configuration directories for your |dir| and |sd| before updating). Note: as the old configuration files are still supported, in most cases no changes are required.

   -  The default configuration does no longer name the :config:option:`Dir/Director`\  and :config:option:`Sd/Storage`\  resources after the systems hostname (:file:`$HOSTNAME-dir` resp. :file:`$HOSTNAME-sd`) but use :config:option:`Dir/Director = bareos-dir`\  resp. :config:option:`Sd/Storage = bareos-sd`\  as defaults. The prior solution had the disadvantage, that :file:`$HOSTNAME-dir` has also been set on |fd| not running on the
      |dir|, which almost ever did require changing this setting. Also the new approach aligns better with :ref:`section-ConfigurationSubdirectories`.

   -  Due to limitation of the build system, the default resource :config:option:`Dir/FileSet = Linux All`\  have been renamed to :config:option:`Dir/FileSet = LinuxAll`\  (no space between Linux and All).

   -  The configuration of the **bareos-traymonitor** has also been split into resource files. Additional, these resource files are now packaged in other packages:

      -  :file:`CONFIGDIR/tray-monitor.d/monitor/bareos-mon.conf`: **bareos-traymonitor**

      -  :file:`CONFIGDIR/tray-monitor.d/client/FileDaemon-local.conf`: **bareos-filedaemon**

      -  :file:`CONFIGDIR/tray-monitor.d/storage/StorageDaemon-local.conf`: **bareos-storage**

      -  :file:`CONFIGDIR/tray-monitor.d/director/Director-local.conf`: :file:`bareos-director`

      This way, the **bareos-traymonitor** will be configured automatically for the installed components.

-  Strict ACL handling

   -  Bareos Console :strong:`Acl`s do no longer automatically matches substrings (to avoid that e.g. :config:option:`dir/console/PoolAcl = Full`\  also matches :config:option:`dir/pool = VirtualFull`\ ). To configure the ACL to work as before, :config:option:`dir/console/PoolAcl = .*Full.*`\  must be set. Unfortunately the |webui| 15.2 :config:option:`Dir/Profile = webui`\  did use
      :config:option:`dir/console/CommandAcl = .bvfs*`\ , which is also no longer works as intended. Moreover, to use all of |webui| 16.2 features, some additional commands must be permitted, so best use the new :config:option:`Dir/Profile = webui-admin`\ .

- |webui|

   -  Updating from Bareos 15.2: Adapt :config:option:`Dir/Profile = webui`\  (from bareos 15.2) to allow all commands of :config:option:`Dir/Profile = webui-admin`\  (:config:option:`dir/console/CommandAcl`\ ). Alternately modify all :config:option:`Dir/Console`\ s currently using :config:option:`Dir/Profile = webui`\  to use :config:option:`Dir/Profile = webui-admin`\  instead.

   -  While RHEL 6 and CentOS 6 are still platforms supported by Bareos, the package **bareos-webui** is not available for these platforms, as the required ZendFramework 2.4 do require PHP >= 5.3.17 (5.3.23). However, it is possible to use **bareos-webui** 15.2 against **bareos-director** 16.2. Also here, the profile must be adapted.

Bareos-15.2
-----------

.. _bareos-15.2.4:

bareos-15.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-15.2.4; Release Notes>`\ 

================ ===============================================
Code Release     2016-06-10
Database Version 2004 (unchanged)
Release Ticket   :mantis:`641`
Url              http://download.bareos.com/bareos/release/15.2/
================ ===============================================

For upgrading from 14.2, please see release notes for 15.2.1.

This release contains several bugfixes and enhancements. Excerpt:

-  Automatic mount of disks by SD

-  NDMP performance enhancements

-  Windows: sparse file restore

-  Director memory leak caused by frequent bconsole calls

.. _bareos-15.2.3:

bareos-15.2.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-15.2.3; Release Notes>`\ 

================ ===============================================
Code Release     2016-03-11
Database Version 2004 (unchanged)
Release Ticket   :mantis:`625`
Url              http://download.bareos.com/bareos/release/15.2/
================ ===============================================

For upgrading from 14.2, please see releasenotes for 15.2.1.

This release contains several bugfixes and enhancements. Excerpt:

-  VMWare plugin can now restore to VMDK file

-  Ceph support for SLES12 included

-  Multiple gfapi and ceph enhancements

-  NDMP enhancements and bugfixes

-  Windows: multiple VSS Jobs can now run concurrently in one FD, installer fixes

-  bpipe: fix stderr/stdout problems

-  reload command enhancements (limitations eliminated)

-  label barcodes now can run without interaction

.. _bareos-15.2.2:

bareos-15.2.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-15.2.2; Release Notes>`\ 

================ ============================================================================================================
Code Release     2015-11-19
Database Version 2004
\                Database update required (if coming from bareos-14.2). See the :ref:`bareos-update` section.
Release Ticket   :mantis:`554`
Url              http://download.bareos.org/bareos/release/15.2/
\                http://download.bareos.com/bareos/release/15.2/
================ ============================================================================================================

First stable release of the Bareos 15.2 branch.

When coming from bareos-14.2.x, the following things have changed (same as in bareos-15.2.1):

-  The default setting for the Bacula Compatbile mode in :config:option:`fd/client/Compatible`\  and :config:option:`sd/storage/Compatible`\  have been changed from :strong:`yes` to :strong:`no`.

-  The configuration syntax for Storage Daemon Cloud Backends Ceph and GlusterFS have changed. Before bareos-15.2, options have been configured as part of the :config:option:`sd/device/ArchiveDevice`\  directive, while now the Archive Device contains only information text and options are defined via the :config:option:`sd/device/DeviceOptions`\  directive. See examples in :config:option:`sd/device/DeviceOptions`\ .

*bareos-15.2.1 (unstable)*
~~~~~~~~~~~~~~~~~~~~~~~~~~

================ ===============================================================================
Code Release     2015-09-16
Database Version 2004
\                Database update required, see the :ref:`bareos-update` section.
Release Ticket   :mantis:`501`
Url              http://download.bareos.org/bareos/release/15.2/
================ ===============================================================================

Beta release.

-  The default setting for the Bacula Compatbile mode in :config:option:`fd/client/Compatible`\  and :config:option:`sd/storage/Compatible`\  have been changed from :strong:`yes` to :strong:`no`.

-  The configuration syntax for Storage Daemon Cloud Backends Ceph and GlusterFS have changed. Before bareos-15.2, options have been configured as part of the :config:option:`sd/device/ArchiveDevice`\  directive, while now the Archive Device contains only information text and options are defined via the :config:option:`sd/device/DeviceOptions`\  directive. See examples in :config:option:`sd/device/DeviceOptions`\ .

Bareos-14.2
-----------

It is known, that :command:`drop_database` scripts will not longer work on PostgreSQL < 8.4. However, as :command:`drop_database` scripts are very seldom needed, package dependencies do not yet enforce PostgreSQL >= 8.4. We plan to ensure this in future version of Bareos.

.. _bareos-14.2.7:

bareos-14.2.7
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.7; Release Notes>`\ 

================ ===============================================
Code Release     2016-07-11
Database Version 2003 (unchanged)
Release Ticket   :mantis:`584`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes. Excerpt:

-  bareos-dir

   -  | Fixes pretty printing of Fileset options block
      | :mantis:`591`: config pretty-printer does not print filesets correctly

   -  | run command: fixes changing the pool when changing the backup level in interactive mode
      | :mantis:`633`: Interactive run doesn’t update pool on level change

   -  | Ignore the Fileset option DriveType on non Windows systems
      | :mantis:`644`: Setting DriveType causes empty backups on Linux

   -  | Suppress already queued jobs for disabled schedules
      | :mantis:`659`: Suppress already queued jobs for disabled schedules

-  NDMP

   -  | Fixes cancel of NDMP jobs
      | :mantis:`604`: Cancel a NDMP Job causes the sd to crash

-  bpipe-fd plugin

   -  | Only take stdout into account, ignore stderr (like earlier versions)
      | :mantis:`632`: fd-bpipe plugin merges stderr with stdout, which can result in corrupted backups

-  win32

   -  | Fix symlink and junction support
      | :mantis:`575`: charset problem in symlinks/junctions windows restore
      | :mantis:`615`: symlinks/junctions wrong target path on restore (wide chars)

   -  | Fixes quoting for bmail.exe in bareos-dir.conf
      | :mantis:`581`: Installer is setting up a wrong path to bmail.exe without quotes / bmail not called

   -  | Fix crash on restore of sparse files
      | :mantis:`640`: File daemon crashed after restoring sparse file on windows

-  win32 mssql plugin

   -  | Allow connecting to non default instance
      | :mantis:`383`: mssqldvi problem with connection to mssql not default instance

   -  | Fix backup/restore of incremental backups
      | :mantis:`588`: Incremental MSSQL backup fails when database name contains spaces

.. _bareos-14.2.6:

bareos-14.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.6; Release Notes>`\ 

================ ===============================================
Code Release     2015-12-03
Database Version 2003 (unchanged)
Release Ticket   :mantis:`474`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes.

.. _bareos-14.2.5:

bareos-14.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.5; Release Notes>`\ 

================ ===============================================
Code Release     2015-06-01
Database Version 2003 (unchanged)
Release Ticket   :mantis:`447`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes and added the platforms :os:`Debian 8` and :os:`Fedora 21`.

.. _bareos-14.2.4:

bareos-14.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.4; Release Notes>`\ 

================ ===============================================
Code Release     2015-03-23
Database Version 2003 (unchanged)
Release Ticket   :mantis:`420`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes, including one major bugfix (:mantis:`437`), relevant for those of you using backup to disk with autolabeling enabled.

It can lead to loss of a 64k block of data when all of this conditions apply:

-  backups are written to disk (tape backups are not affected)

-  autolabelling is enabled

-  a backup spans over multiple volumes

-  the additional volumes are newly created and labeled during the backup

If existing volumes are used for backups spanning over multiple volumes, the problem does not occur.

We recommend to update to the latest packages as soon as possible.

If an update is not possible immediately, autolabeling should be disabled and volumes should be labelled manually until the update can be installed.

If you are affected by the 64k bug, we recommend that you schedule a full backup after fixing the problem in order to get a proper full backup of all files.

.. _bareos-14.2.3:

bareos-14.2.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.3; Release Notes>`\ 

================ ===============================================
Code Release     2015-02-02
Database Version 2003 (unchanged)
Release Ticket   :mantis:`393`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

.. _bareos-14.2.2:

bareos-14.2.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.2; Release Notes>`\ 

================ =================================================================
Code Release     2014-12-12
Database Version 2003 (unchanged)
\                Database update required if updating from version < 14.2.
\                See the :ref:`bareos-update` section for details.
Url              http://download.bareos.org/bareos/release/14.2/
\                http://download.bareos.com/bareos/release/14.2/
================ =================================================================

First stable release of the Bareos 14.2 branch.

*bareos-14.2.1 (unstable)*
~~~~~~~~~~~~~~~~~~~~~~~~~~

================ ===============================================================================
Code Release     2014-09-22
Database Version 2003
\                Database update required, see the :ref:`bareos-update` section.
Url              http://download.bareos.org/bareos/release/14.2/
================ ===============================================================================

Beta release.

Bareos-13.2
-----------

.. _bareos-13.2.5:

bareos-13.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.5; Release Notes>`\ 

================ ===============================================
Code Release     2015-12-03
Database Version 2002 (unchanged)
Url              http://download.bareos.com/bareos/release/13.2/
================ ===============================================

This release contains several bugfixes.

.. _bareos-13.2.4:

bareos-13.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.4; Release Notes>`\ 

================ ===============================================
Code Release     2014-11-05
Database Version 2002 (unchanged)
Url              http://download.bareos.com/bareos/release/13.2/
================ ===============================================

.. _bareos-13.2.3:

bareos-13.2.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.3; Release Notes>`\ 

================ ===============================================================================
Code Release     2014-03-11
Database Version 2002
\                Database update required, see the :ref:`bareos-update` section.
Url              http://download.bareos.com/bareos/release/13.2/
================ ===============================================================================

It is known, that :command:`drop_database` scripts will not longer work on PostgreSQL < 8.4. However, as :command:`drop_database` scripts are very seldom needed, package dependencies do not yet enforce PostgreSQL >= 8.4. We plan to ensure this in future version of Bareos.

.. _bareos-13.2.2:

bareos-13.2.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.2; Release Notes>`\ 

================ ===============================================
Code Release     2013-11-19
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/13.2/
\                http://download.bareos.com/bareos/release/13.2/
================ ===============================================

Bareos-12.4
-----------

.. _bareos-12.4.8:

bareos-12.4.8
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.8; Release Notes>`\ 

================ ===============================================
Code Release     2015-11-18
Database Version 2001 (unchanged)
Url              http://download.bareos.com/bareos/release/12.4/
================ ===============================================

This release contains several bugfixes.

.. _bareos-12.4.6:

bareos-12.4.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.6; Release Notes>`\ 

================ ===============================================
Code Release     2013-11-19
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.5:

bareos-12.4.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.5; Release Notes>`\ 

================ ===============================================
Code Release     2013-09-10
Database Version 2001 (unchanged)
Url              http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.4:

bareos-12.4.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.4; Release Notes>`\ 

================ ===============================================
Code Release     2013-06-17
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.3:

bareos-12.4.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.3; Release Notes>`\ 

================ ===============================================
Code Release     2013-04-15
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.2:

bareos-12.4.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.2; Release Notes>`\ 

================ ================
Code Release     2013-03-03
Database Version 2001 (unchanged)
================ ================

.. _bareos-12.4.1:

bareos-12.4.1
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.1; Release Notes>`\ 

================ ==============
Code Release     2013-02-06
Database Version 2001 (initial)
================ ==============

This have been the initial release of Bareos.

Information about migrating from Bacula to Bareos are available at `Howto upgrade from Bacula to Bareos <http://www.bareos.org/en/HOWTO/articles/upgrade_bacula_bareos.html>`_ and in section :ref:`compat-bacula`.

.. _releasenotes:

Release Notes
=============

:index:`\ <single: Releases>`

The technical changelog is automatically generated from the Bareos bug tracking system, see http://bugs.bareos.org/changelog_page.php.

Please note, that some of the subreleases are only internal development releases.

Open issues for a specific version are shown at http://bugs.bareos.org/roadmap_page.php.

The overview about new feature of a release are shown at https://github.com/bareos/bareos and in the :ref:`index` of this document.

This chapter concentrates on things to do when updating an existing Bareos installation.



   .. warning::

      While all the source code is published on `GitHub <https://github.com/bareos/bareos>`_, the releases of packages on http://download.bareos.org is limited to the initial versions of a major release. Later maintenance releases are only published on https://download.bareos.com.

Bareos-17.2
-----------

.. _bareos-17.2.7:

bareos-17.2.7
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.7; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2018-07-13\\
   Database Version  & 2171 (unchanged)\\
   Release Ticket    & :ticket:`966`\\
   Url               & http://download.bareos.com/bareos/release/17.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2018-07-13
Database Version 2171 (unchanged)
Release Ticket   :ticket:`966`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  :ticket:`892` **bareos-storage-droplet**: improve error handling on unavailable backend.

-  :ticket:`902` **bareos-storage-droplet**: improve job status handling (terminate job after all data is written).

-  :ticket:`967` :os:`Windows`: overwrite symbolic links on restore.

-  :ticket:`983` |sd|: prevent sporadic crash when :config:option:`sd/storage/CollectJobStatistics = yes`\ .

-  :os:`SLES 12sp2` and :os:`SLES 12sp3`: provide **bareos-storage-ceph** and **bareos-filedaemon-ceph-plugin** packages.

.. _bareos-17.2.6:

bareos-17.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.6; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2018-06-21\\
   Database Version  & 2171 (unchanged)\\
   Release Ticket    & :ticket:`916`\\
   Url               & http://download.bareos.com/bareos/release/17.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2018-06-21
Database Version 2171 (unchanged)
Release Ticket   :ticket:`916`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  added platforms: :os:`Fedora 27`, :os:`Fedora 28`, :os:`openSUSE 15.0`, :os:`Ubuntu 18.04` and :os:`Univention 4.3`.

-  :os:`Univention 4.3`: fixes integration.

-  :ticket:`872` adapted to new Ceph API.

-  :ticket:`943` use **tirpc** if Sun-RPC is not provided.

-  :ticket:`964` fixes the predefined queries.

-  :ticket:`969` fixes a problem of restoring more files then selected in |webui|/BVFS.

-  |dir|: fixes for a crash after reload in the statistics thread (:ticket:`695`, :ticket:`903`).

-  :command:`bareos-dbcheck`: cleanup and speedup for some some of the checks.

-  adapted for |postgresql| 10.

-  gfapi: stale file handles are treated as warnings

.. _bareos-17.2.5:

bareos-17.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.5; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2018-02-16\\
   Database Version  & 2171 (unchanged)\\
   Release Ticket    & :ticket:`910`\\
   Url               & http://download.bareos.com/bareos/release/17.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2018-02-16
Database Version 2171 (unchanged)
Release Ticket   :ticket:`910`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  |fd| is ready for :os:`AIX 7.1.0.0`.

-  :ref:`VMwarePlugin` is also provided for :os:`Debian 9`.

-  NDMP fixes

-  Virtual Backup fixes

-  **bareos-storage-droplet**: improvements

-  :command:`bareos-dbcheck` improvements and fixes: with older versions it could happen, that it destroys structures required by :bcommand:`.bvfs_*`.

-  :ticket:`850` fixes a bug on :os:`Univention`: fixes a problem of regenerating passwords when resyncing settings.

-  :ticket:`890` :bcommand:`.bvfs_update` fix. Before there have been cases where it did not update the cache.

-  :bcommand:`.bvfs_lsdirs` make limit- and offset-option work correctly.

-  :bcommand:`.bvfs_lsdirs` show special directory (like :file:`@bpipe@/`) on the same level as :file:`/`.

-  :ticket:`895` added description to the output of :bcommand:`show filesets`.

-  |webui|: Restore Browser fixes

   -  There was the possibility of an endless loop if the BVFS API delivers unexpected results. This has been fixed. See bugreports :ticket:`887` and :ticket:`893` for details.

   -  :ticket:`905` fixes a problem with file names containing quotes.

-  :config:option:`dir/client/NdmpBlockSize`\  changed type from :strong:`Pint32` to :strong:`Size32`. This should not affect any configuration, but is more consistent with other block size configuration directives.

.. _bareos-17.2.4:

bareos-17.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-17.2.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2017-12-14\\
   Database Version  & 2171\\
   Release Ticket    & :ticket:`861`\\
   Url               & http://download.bareos.org/bareos/release/17.2/ \\
                     & http://download.bareos.com/bareos/release/17.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2017-12-14
Database Version 2171
Release Ticket   :ticket:`861`
Url              http://download.bareos.org/bareos/release/17.2/
\                http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several enhancements. Excerpt:

-  Bareos Distribution (packages)

   -  **python-bareos** is included in the core distribution.

   -  **bareos-storage-droplet** is a storage backend for the droplet library. Most notably it allows backup and restores to a S3 environment. \betaSince{sd}{bareos-storage-droplet}{17.2.4}

   -  **bat** has been removed, see section :ref:`bat`.

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

-  :ticket:`629` Windows: restore directory attributes.

-  :ticket:`639` tape: fix block size handling, AWS VTL iSCSI devices

-  :ticket:`705` support for MySQL 5.7

-  :ticket:`719` allow long JSON messages (has been increased from 100KB to 2GB).

-  :ticket:`793` Virtual Backups: skip jobs with no files.

Bareos-16.2
-----------

.. _bareos-16.2.8:

bareos-16.2.8
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.8; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2018-07-06\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`863`\\
   Url               & http://download.bareos.com/bareos/release/16.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2018-07-06
Database Version 2004 (unchanged)
Release Ticket   :ticket:`863`
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

-  :ticket:`967` Windows: Symbolic links are now replaceable during restore

.. _bareos-16.2.7:

bareos-16.2.7
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.7; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2017-10-09\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`836`\\
   Url               & http://download.bareos.com/bareos/release/16.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2017-10-09
Database Version 2004 (unchanged)
Release Ticket   :ticket:`836`
Url              http://download.bareos.com/bareos/release/16.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  Fixes a Director crash, when enabling debugging output

-  :bcommand:`.bvfs_lsdirs`: improve performance, especially when having a large number of directories

   -  To optimize the performance of the SQL query used by :bcommand:`.bvfs_lsdirs`, it is important to have the following indexes:

   -  PostgreSQL

      -  

         ``CREATE INDEX file_jpfnidpart_idx ON File(PathId,JobId,FilenameId) WHERE FileIndex = 0;``

      -  | If the index ``file_jfnidpart_idx`` mentioned in 16.2.6 release notes exist, drop it:
         | ``DROP INDEX file_jfnidpart_idx;``

   -  MySQL/MariaDB

      -  

         ``CREATE INDEX PathId_JobId_FileNameId_FileIndex ON File(PathId,JobId,FilenameId,FileIndex);``

      -  | If the index ``PathId_JobId_FileIndex_FileNameId`` mentioned in 16.2.6 release notes exist, drop it:
         | ``DROP INDEX PathId_JobId_FileIndex_FileNameId ON File;``

-  Utilize OpenSSL >= 1.1 if available

-  Windows: fixes silent upgrade (:command:`winbareos-*.exe /S`)

-  Windows: restore attributes also on directories (not only on files)

-  Fixes problem with SHA1 signature when compiled without OpenSSL (not relevant for bareos.org/bareos.com packages)

-  Packages for openSUSE Leap 42.3 and Fedora 26 have been added.

-  Packages for AIX and current HP-UX 11.31

.. _bareos-16.2.6:

bareos-16.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-16.2.6; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2017-06-22\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`794`\\
   Url               & http://download.bareos.com/bareos/release/16.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2017-06-22
Database Version 2004 (unchanged)
Release Ticket   :ticket:`794`
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

      -  | When using PostgreSQL, creating the following partial improves the performance sufficiently:
         | ``CREATE INDEX file_jfnidpart_idx ON File(JobId, FilenameId) WHERE FileIndex = 0;``

      -  | Run following command to create the partial index:
         | :file:`su - postgres -c 'echo "CREATE INDEX file_jfnidpart_idx ON File(JobId, FilenameId) WHERE FileIndex = 0; ANALYZE File;" | psql bareos'`

   -  

      |mysql|

      -  | When using MySQL or MariaDB, creating the following index improves the performance:
         | ``CREATE INDEX PathId_JobId_FileIndex_FileNameId ON File(PathId,JobId,FileIndex,FilenameId);``

      -  | Run following command to create the index:
         | :file:`echo "CREATE INDEX PathId_JobId_FileIndex_FileNameId ON File(PathId,JobId,FileIndex,FilenameId);" | mysql -u root bareos`

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

:index:`\ <single: bareos-16.2.5; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2017-03-03\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`734`\\
   Url               & http://download.bareos.com/bareos/release/16.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2017-03-03
Database Version 2004 (unchanged)
Release Ticket   :ticket:`734`
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

:index:`\ <single: bareos-16.2.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2016-10-28\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`698`\\
   Url               & http://download.bareos.org/bareos/release/16.2/ \\
                     & http://download.bareos.com/bareos/release/16.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2016-10-28
Database Version 2004 (unchanged)
Release Ticket   :ticket:`698`
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

-  

   |webui|

   -  Updating from Bareos 15.2: Adapt :config:option:`Dir/Profile = webui`\  (from bareos 15.2) to allow all commands of :config:option:`Dir/Profile = webui-admin`\  (:config:option:`dir/console/CommandAcl`\ ). Alternately modify all :config:option:`Dir/Console`\ s currently using :config:option:`Dir/Profile = webui`\  to use :config:option:`Dir/Profile = webui-admin`\  instead.

   -  While RHEL 6 and CentOS 6 are still platforms supported by Bareos, the package **bareos-webui** is not available for these platforms, as the required ZendFramework 2.4 do require PHP >= 5.3.17 (5.3.23). However, it is possible to use **bareos-webui** 15.2 against **bareos-director** 16.2. Also here, the profile must be adapted.

Bareos-15.2
-----------

.. _bareos-15.2.4:

bareos-15.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-15.2.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2016-06-10\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`641` \\
   Url               & http://download.bareos.com/bareos/release/15.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2016-06-10
Database Version 2004 (unchanged)
Release Ticket   :ticket:`641`
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

:index:`\ <single: bareos-15.2.3; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2016-03-11\\
   Database Version  & 2004 (unchanged)\\
   Release Ticket    & :ticket:`625` \\
   Url               & http://download.bareos.com/bareos/release/15.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2016-03-11
Database Version 2004 (unchanged)
Release Ticket   :ticket:`625`
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

:index:`\ <single: bareos-15.2.2; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-11-19\\
   Database Version  & 2004\\
                     & Database update required (if coming from bareos-14.2). See the :ref:`bareos-update` section.\\
   Release Ticket    & :ticket:`554` \\
   Url               & http://download.bareos.org/bareos/release/15.2/ \\
                     & http://download.bareos.com/bareos/release/15.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ============================================================================================================
Code Release     2015-11-19
Database Version 2004
\                Database update required (if coming from bareos-14.2). See the :ref:`bareos-update` section.
Release Ticket   :ticket:`554`
Url              http://download.bareos.org/bareos/release/15.2/
\                http://download.bareos.com/bareos/release/15.2/
================ ============================================================================================================

First stable release of the Bareos 15.2 branch.

When coming from bareos-14.2.x, the following things have changed (same as in bareos-15.2.1):

-  The default setting for the Bacula Compatbile mode in :config:option:`fd/client/Compatible`\  and :config:option:`sd/storage/Compatible`\  have been changed from :strong:`yes` to :strong:`no`.

-  The configuration syntax for Storage Daemon Cloud Backends Ceph and GlusterFS have changed. Before bareos-15.2, options have been configured as part of the :config:option:`sd/device/ArchiveDevice`\  directive, while now the Archive Device contains only information text and options are defined via the :config:option:`sd/device/DeviceOptions`\  directive. See examples in :config:option:`sd/device/DeviceOptions`\ .

*bareos-15.2.1 (unstable)*
~~~~~~~~~~~~~~~~~~~~~~~~~~

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-09-16\\
   Database Version  & 2004\\
                     & Database update required, see the :ref:`bareos-update` section.\\
   Release Ticket    & :ticket:`501` \\
   Url               & http://download.bareos.org/bareos/release/15.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================================================
Code Release     2015-09-16
Database Version 2004
\                Database update required, see the :ref:`bareos-update` section.
Release Ticket   :ticket:`501`
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

:index:`\ <single: bareos-14.2.7; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2016-07-11\\
   Database Version  & 2003 (unchanged)\\
   Release Ticket    & :ticket:`584` \\
   Url               & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2016-07-11
Database Version 2003 (unchanged)
Release Ticket   :ticket:`584`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes. Excerpt:

-  bareos-dir

   -  | Fixes pretty printing of Fileset options block
      | :ticket:`591`: config pretty-printer does not print filesets correctly

   -  | run command: fixes changing the pool when changing the backup level in interactive mode
      | :ticket:`633`: Interactive run doesn’t update pool on level change

   -  | Ignore the Fileset option DriveType on non Windows systems
      | :ticket:`644`: Setting DriveType causes empty backups on Linux

   -  | Suppress already queued jobs for disabled schedules
      | :ticket:`659`: Suppress already queued jobs for disabled schedules

-  NDMP

   -  | Fixes cancel of NDMP jobs
      | :ticket:`604`: Cancel a NDMP Job causes the sd to crash

-  bpipe-fd plugin

   -  | Only take stdout into account, ignore stderr (like earlier versions)
      | :ticket:`632`: fd-bpipe plugin merges stderr with stdout, which can result in corrupted backups

-  win32

   -  | Fix symlink and junction support
      | :ticket:`575`: charset problem in symlinks/junctions windows restore
      | :ticket:`615`: symlinks/junctions wrong target path on restore (wide chars)

   -  | Fixes quoting for bmail.exe in bareos-dir.conf
      | :ticket:`581`: Installer is setting up a wrong path to bmail.exe without quotes / bmail not called

   -  | Fix crash on restore of sparse files
      | :ticket:`640`: File daemon crashed after restoring sparse file on windows

-  win32 mssql plugin

   -  | Allow connecting to non default instance
      | :ticket:`383`: mssqldvi problem with connection to mssql not default instance

   -  | Fix backup/restore of incremental backups
      | :ticket:`588`: Incremental MSSQL backup fails when database name contains spaces

.. _bareos-14.2.6:

bareos-14.2.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.6; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-12-03\\
   Database Version  & 2003 (unchanged)\\
   Release Ticket    & :ticket:`474` \\
   Url               & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-12-03
Database Version 2003 (unchanged)
Release Ticket   :ticket:`474`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes.

.. _bareos-14.2.5:

bareos-14.2.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.5; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-06-01\\
   Database Version  & 2003 (unchanged)\\
   Release Ticket    & :ticket:`447` \\
   Url               & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-06-01
Database Version 2003 (unchanged)
Release Ticket   :ticket:`447`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes and added the platforms :os:`Debian 8` and :os:`Fedora 21`.

.. _bareos-14.2.4:

bareos-14.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-03-23 \\
   Database Version  & 2003 (unchanged)\\
   Release Ticket    & :ticket:`420` \\
   Url               & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-03-23
Database Version 2003 (unchanged)
Release Ticket   :ticket:`420`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

This release contains several bugfixes, including one major bugfix (:ticket:`437`), relevant for those of you using backup to disk with autolabeling enabled.

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

:index:`\ <single: bareos-14.2.3; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-02-02 \\
   Database Version  & 2003 (unchanged)\\
   Release Ticket    & :ticket:`393`\\
   Url               & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-02-02
Database Version 2003 (unchanged)
Release Ticket   :ticket:`393`
Url              http://download.bareos.com/bareos/release/14.2/
================ ===============================================

.. _bareos-14.2.2:

bareos-14.2.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-14.2.2; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2014-12-12 \\
   Database Version  & 2003 (unchanged)\\
                     & Database update required if updating from version < 14.2.\\
                     & See the :ref:`bareos-update` section for details.\\
   Url               & http://download.bareos.org/bareos/release/14.2/ \\
                     & http://download.bareos.com/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

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

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release & 2014-09-22 \\
   Database Version  & 2003\\
                     & Database update required, see the :ref:`bareos-update` section.\\
   Url               & http://download.bareos.org/bareos/release/14.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

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

:index:`\ <single: bareos-13.2.5; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-12-03 \\
   Database Version  & 2002 (unchanged)\\
   Url               & http://download.bareos.com/bareos/release/13.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-12-03
Database Version 2002 (unchanged)
Url              http://download.bareos.com/bareos/release/13.2/
================ ===============================================

This release contains several bugfixes.

.. _bareos-13.2.4:

bareos-13.2.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2014-11-05 \\
   Database Version  & 2002 (unchanged)\\
   Url               & http://download.bareos.com/bareos/release/13.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2014-11-05
Database Version 2002 (unchanged)
Url              http://download.bareos.com/bareos/release/13.2/
================ ===============================================

.. _bareos-13.2.3:

bareos-13.2.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-13.2.3; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2014-03-11 \\
   Database Version  & 2002\\
                     & Database update required, see the :ref:`bareos-update` section.\\
   Url               & http://download.bareos.com/bareos/release/13.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

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

:index:`\ <single: bareos-13.2.2; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-11-19 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.org/bareos/release/13.2/ \\
                     & http://download.bareos.com/bareos/release/13.2/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

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

:index:`\ <single: bareos-12.4.8; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2015-11-18 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.com/bareos/release/12.4/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2015-11-18
Database Version 2001 (unchanged)
Url              http://download.bareos.com/bareos/release/12.4/
================ ===============================================

This release contains several bugfixes.

.. _bareos-12.4.6:

bareos-12.4.6
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.6; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-11-19 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.org/bareos/release/12.4/ \\
                     & http://download.bareos.com/bareos/release/12.4/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2013-11-19
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.5:

bareos-12.4.5
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.5; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-09-10 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.com/bareos/release/12.4/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2013-09-10
Database Version 2001 (unchanged)
Url              http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.4:

bareos-12.4.4
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.4; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-06-17 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.org/bareos/release/12.4/ \\
                     & http://download.bareos.com/bareos/release/12.4/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2013-06-17
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.3:

bareos-12.4.3
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.3; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-04-15 \\
   Database Version  & 2001 (unchanged)\\
   Url               & http://download.bareos.org/bareos/release/12.4/ \\
                     & http://download.bareos.com/bareos/release/12.4/ \\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ===============================================
Code Release     2013-04-15
Database Version 2001 (unchanged)
Url              http://download.bareos.org/bareos/release/12.4/
\                http://download.bareos.com/bareos/release/12.4/
================ ===============================================

.. _bareos-12.4.2:

bareos-12.4.2
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.2; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-03-03 \\
   Database Version  & 2001 (unchanged)\\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ================
Code Release     2013-03-03
Database Version 2001 (unchanged)
================ ================

.. _bareos-12.4.1:

bareos-12.4.1
~~~~~~~~~~~~~

:index:`\ <single: bareos-12.4.1; Release Notes>`

# Tabular in LaTex format (original)

::

   \begin{tabular}{p{0.2\textwidth} p{0.8\textwidth}}
   Code Release      & 2013-02-06 \\
   Database Version  & 2001 (initial)\\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

================ ==============
Code Release     2013-02-06
Database Version 2001 (initial)
================ ==============

This have been the initial release of Bareos.

Information about migrating from Bacula to Bareos are available at `Howto upgrade from Bacula to Bareos <http://www.bareos.org/en/HOWTO/articles/upgrade_bacula_bareos.html>`_ and in section :ref:`compat-bacula`.





[BAREOS](http://www.bareos.org/) is a fork of the Bacula project.

If you want to contribute somethings to this repo please read our contribution
policy at https://www.bareos.org/en/howto-contribute.html

This source code has the following changes (highlevel) compared with
Bacula 5.2.13 (original version forked.):

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=master)](https://travis-ci.org/bareos/bareos/branches)

Release 17.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-17.2)](https://travis-ci.org/bareos/bareos/branches)

* Performance improvements through denormalization of the ```File``` database table
  * The denormalization of the File database table leads to enormous performance improvements in installation, which covering a lot of ﬁle (millions and more).
  * For the denormalization the database schema must be modiﬁed. Please note! Updating the database to schema version >= 2170 will increase the required disk space. Especially it will require around twice the amount of the current database disk space during the migration.
  * The ```Filename``` database table does no longer exists. Therefore the ```.bvfs_*``` commands do no longer output the ```FilenameId``` column.
* ```NDMP_NATIVE``` support has been added. This include the NDMP features DAR and DDAR.
* Added Bareos Storage Daemon backend droplet. Most notably it allows backup and restores to a S3 environment. Included as beta. Don’t use in productive environment!
* Soft Quota: automatic quota grace period reset if a job does not exceed the quota.
* bareos-dbcheck: disable all interactive questions in batch mode.
* list files: also show deleted ﬁles (accurate mode).
* list jobstatastics: added.
* purge : added conﬁrmation.
* list volumes: fix limit and offset handling.
* [Windows: restore directory attributes](https://bugs.bareos.org/view.php?id=629)
* [Tape: ﬁx block size handling, AWS VTL iSCSI devices](https://bugs.bareos.org/view.php?id=639)
* [Support for MySQL 5.7](https://bugs.bareos.org/view.php?id=705)
* [Allow long JSON messages (has been increased from 100KB to 2GB)](https://bugs.bareos.org/view.php?id=719)
* [Virtual Backups: skip jobs with no ﬁles](https://bugs.bareos.org/view.php?id=793)
* BAT has been removed. Please use [bareos-webui](https://github.com/bareos/bareos-webui/) instead.
* Bareos File Daemon now also is ready for
  * MacOS
  * HP-UX 11.31 (ia64)

Release 16.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-16.2)](https://travis-ci.org/bareos/bareos/branches)

* Always Incremental Backup
* Client initiated connection (active client behind firewall)
* Enhanced ACLs: multi-tenancy capability
* NDMP Enhancements
* New subdirectory configuration scheme to ease automation
* Director Configuration API
  * Initially only adding configuration is implemented
* API enhancements for Bareos WebUI
* BAT is marked as deprecated in favor of Bareos WebUI


Release 16.1.x
==============

* Remove fixed size reload table
* Get rid of one VSS client at a time
   * Allow multiple jobs to run on Windows

Release 15.4.x
==============

* Storage Daemon Refactor record write code path.
* Enable concurrent jobs on Windows platforms
* Report the secure erase settings in the jobreport
* Deprecated POOLMEM \*\* in favor of POOLMEM \*\&
* One command loop for normal and dot commands
* BVFS remove set\_username()
* BSOCK allow sending long messages
* Cleanup reload\_config command
   * reload config command is callable as signal handler for SIGHUP
     and by the bconsole reload command.
   * Make sure, that the SIGHUP handler is only called once.
   * Do not terminate the director in case of config errors.

Release 15.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-15.2)](https://travis-ci.org/bareos/bareos/branches)

* Lots of refactoring of old code.
* Systemd support for Debian >= 8 and Ubuntu >= 15.04
* Volume manager cleanup in storage daemon.
* New JSON API support using JANSSON for director
* Replace own JSON representation code with JANSSON
* Allow scanning and storing restore objects with bscan.
* Unit tests created using CMOCKA
* Add support for RESTORE\_OBJECTS in verify jobs
* Windows API fixes (deduped file backup, compressed file attributes)
* Windows build got more executable (grow, bscan, bwild, bregex)
* Windows binaries are code signed.
* Fixes to FD plugin framework to fix original mis-design.
* Rerun overhaul
   * Rerun now also works with virtual full backups.
   * Only allow rerun of Backup/Copy/Migration Jobs
* Storage daemon reservation logic enhanced to exchange already tried
  volume names when reservation fails of the earlier returned volume name.
* Fileset encryption overhaul.
   * Add new fileset option that forces encryption.
   * Implement the encryption=<cipher> fileset option
* Make resource propagation a standard operation.
* Refactor storage daemon flag handling.
   * Changed all defines to an enum and switch to using bit operations.
* Preliminary code to make database connection more robust
   * Allow reconnect of database connection when no transaction is pending.
   * Allow setting of database connection failure to be fatal.
   * Both disabled by default.
* Debug printing enhancements:
   * Increase debug message timestamp resolution to include usecs.
   * When printing protocol messages drop printing \n
   * Set JCR in TSD for stored and filed.
* NDMP enhancements
   * Reference Implementation enhancements
   * Single file restore fixes
   * Incremental backups and restores fixed
   * Relocation on restore fixed

Release 15.1.x
==============

* Refactoring of Migrate/Copy selection code. (Mantis #342)
* Major TLS overhaul. (Mantis #371)
* Rados (CEPH) File Daemon plugin
* GFAPI (GlusterFS) File daemon plugin
* CEPHFS (CEPH) File daemon plugin
* Cloud plugins switched to use device options config string.
* Escape parsing in generic plugin option parser.

Release 14.4.x
==============

* Support to rados SD backend for libradosstriper
* Refactor socket handling.

(Most features from this release got backported to 14.2.x)

Release 14.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-14.2)](https://travis-ci.org/bareos/bareos/branches)

* Configuration engine refactoring in preparation for configuration API.
* Accurate mode refactoring
   * Data storage abstraction
   * In memory hashtable (same as previously)
   * LMDB (Lightning Memory DB same as used in OpenLDAP.)
* Python FD plugin base class abstraction for Python plugin writers.
* Added missing methods to Python FD class should now have all methods and structures that C plugin has.
* Fixed most important high level coding problems found by Coverity scans.
* Disable a client (analog to disable job but then for all jobs of a certain client.)
* Disable a schedule (analog to disable job but then for all jobs triggered by that schedule.)
* Cleanup of more global variables.
* Make scripting more robust for getting the database engine used.
* Debian packaging got major overhaul.
   * dbconfig used for database configuration on debian based distributions.
   * dh\_installinit used for init scripts on debian based distributions.
   * The LICENSE text is more inline with what debian requires for the debian packaging.
* Bug fixes to bugs found by Coverity scans.
* Added prototype of CRL (Certificate Revocation List) reload logic for openssl.
* Limit storage list to autochangers automatically when a command makes sense only on autochangers.
* Exclude Dir containting extended to allow multiple settings.
* Added elapsed time printing in restore jobs.
* Some tools are installed into bindir instead of sbindir now.
* Traymonitor now shows a red icon when there are connection or authentication problems with a server.
* CentOS 7 and RHEL7 support added to build system.
* Fixed several problems with copy and migration jobs to make them work better.
   * Do not cancel the copy/migration job on certain events.
   * When you configure your Copy and Migration Jobs now without a dummy client/fileset you get the following:
      * Jobs now show the actual client of the original job.
      * Jobs now show the actual level of the the original job.
      * Jobs now show the actual fileset of the original job.
   * The jobs data spooling setting is now only used when its not enabled already.
      * In the old situation the disabling of data spooling in the Job would disable a command line override to enable spooling.
   * The printing of Client and FileSet and Level is restored now they are set with sensible info.
   * Disabled checking client concurrency for Copy and Migration Jobs.
      * As the client is not contacted anyway we are now only limited by the storage concurrency.
   * Allow to read from file type devices multiple times
      * A read volume on a file type device can now be reserved by multiple jobs and when the jobs
        use the same volume they can execute at the same time when the right concurrency is met on the other resources.
* Added a prototype of a CEPHFS storage backend.
* Added support for dynamic loading of storage backends in the storage daemon.
   * File storage is always loaded.
   * Tape/GFAPI/RADOS/... can be loaded dynamically (simular to what we alread had for database backends.)
* All shared code between the different storage programs (bareos-sd, btape, ...) is now in a new shared library.
* VTAPE is removed from the code base (Use MHVTL).
* Initial support for TAPEALERT is added.
* Storage daemon statistics gathering.
* Some preliminary work for allowing to have audit events.
* Use a special backend dir to store both catalog and storage backends.
* Support for NotToBackup Registry Key on windows.
* On the packaging side we now store the bareos libs in a subdir. (e.g. /usr/lib/bareos, /usr/lib64/bareos)
* Allow multiple python DIR and SD plugins.
* Class based Python DIR plugin wrappers.
* Max VirtualFull Interval and Virtual Full Pool override.
* Syslog message destinations fixed, allow setting of syslog facility and correct log level.
* Add ability to specify server address for ADO connect string in mssqlvdi plugin.
* Refactor FileSet Options using bit operations (future readiness)
* ANSI label compatibility with Bacula ANSI labels.

Release 14.1.x
==============

* Windows DIR.
* Support for different blocksizes (e.g. per volume blocksize) for better performance.
* Preliminary support for Cloud storage backends:
   * GFAPI (Gluster FileSystem) (tested simple backup and restore, needs more tests)
   * Object storage (S3/Swift etc.) using libdroplet (Needs patched bareos libdroplet) (Due to unusable VFS abstraction not working).
   * Rados (CEPH) (tested simple backup and restore, needs more tests)

Release 13.4.x
==============

* First attempt at supporting IBM lin\_tape driver.
* Fix storage daemon plugin interface.
* AIX compile fixes.
* Catalog backend scripting reworked.
* Python plugins (fd/sd/dir)
* Windows SD.
* Support for Univention packages.
* Bpipe plugin upgraded to new plugin config parser.
* Socket abstraction (TCP supported, prototype for SCTP and UDT sockets).
* Windows enhancements
   * Volume Mountpoints support (VMP).
   * Use GetFileInformationByHandleEx to get real changetime.
   * Windows dedup support
   * Initial support for Windows EFS filesystems

Release 13.3.x
==============

* NDMP enhancements (first code for doing filelevel restores, preliminary).
* Plugin framework enhancements.
* Auto inflation/deflation storage daemon plugin (needs more testing).
* Interactive path selection refactoring.
* Add config option for storing a diagnostic device.
* Plugin framework enhancements.
* Conditionally enable VSS.
* Refactor windows version detection code.
* Implement plugin options passing.
* Upgrade all filed plugins to use fd\_common.h
* Windows config files admin-only access.
* Use modern autoconf idioms.
* Enhanced help for run and restore command.
* Add command for doing a name resolution on client.
* Implement a store\_alist\_dir function.
* Fill the process environment with an BAREOS\_CFGDIR.
* Cleanup win api compat layer.
* Sortable tables in bRestore.
* Copy job definition requires unnecessary settings (relax config).
* Added usage to the plugin information block.
* Make new diskonly configs the default installed configs.
* Private connection to database for some long running operations.
* Plugin to do backup of Microsoft Sql Server (mssql) server.
* Config engine redesign Phase 1.

Release 13.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-13.2)](https://travis-ci.org/bareos/bareos/branches)

* Allow to cancel multiple jobids with cancel cmd from bconsole.

Release 13.1.x
==============

* Allow cancel by JobId on storage daemon. (#13)
* Security (filed)
   * Implementation of an allowed scriptdir keyword (#31)
   * Implementation of an allowed job command keyword (#32)
* Cleanup of compression framework (#34)
* Encryption cipher is hardcoded (#46)
* Job migration/copy between different SDs (#7)
* Use open\_bpipe wrapper in bpipe plugin (#104)
   * Side effect of using open\_bpipe is that you can only call
     a binary or a script like all other things in Bareos e.g.
     runscripts etc. If you want to use a direct cmdline you need
     to embed the cmdline in a sh -c 'cmdline ...'
* Add firewall friendly reverse data channel initialization.
   * instead of FD connecting to SD for backup, restore and verify
     the client is defined as a passive client and the Director instructs
     the SD to connect to the FD. So no need for puching holes in your firewall
     for returning data traffic but initiate all connections from the inside to
     the outside. The default is still non passive mode but for external clients
     in an DMZ or on the Internet you can define the client as a passive client.

Release 12.4.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-12.4)](https://travis-ci.org/bareos/bareos)

* First attempt at adding a rerun command.
* Pass the restore context as parameter to display\_job\_parameters
* When showing the pool source show if it was set on the cmdline.
* Link bacula using libumem on Solaris the better multithreaded allocator
* Rewrite find\_one\_file using inline functions so its better readable.
* Add support for backtrace dumps on Solaris
* Add a bstrinlinecpy function which allows for overlapping copies.
* Major overhaul of configure subsystem.
* Rewrite the restore stream processing somewhat so its better readable.
* Use bstr functions whereever possible.
* Major overhaul of config tables and layout of code.
* Backport catalog changes
* Backport of dynamic loadable backends and sql pooling.
* First attempt at adding a fileset size option.
* Replace the mntent cache htable with a dlist.
* Add safeguard to append\_file function for empty filepatterns.
* Modify block device restore
* First attempt at adding fileset shadowing detection.
* Reindent cmdline table.
* Reindent config tables without space allignment.
* Add sd and dir plugin dir to build targets.
* Do not check for backend when --enable-client-only is given.
* Backported quota support.
* Import/Export of volumes from bconsole and BAT with full support in
  bareos-dir and bareos-sd
  * Cleanup of dird
  * Add an autochanger transfer option.
  * Add extra function to update database with new data.
  * Allow get\_user\_slot\_list to search for specific argument.
  * Tweak layout of status slots
  * Tweak status slots no need to get a drive
  * Tweak update slots only ask for drive with scan option.
  * First try at implementing Import/Export GUI.
  * Add better support for multi drive autochangers.
  * Change drive selection dialog.
  * Allow export when a volume is loaded in a drive.
  * Repopulate the media view on enabling the pane.
  * Do not show import/export slots in contents overview.
  * Repopulate panes in BAT when they get focus again.
  * Allow a move slots operation from BAT.
* Upgrade libtool to 2.4.2
* Bring autoconf stuff to 2.68 version.
* See if adding ENGINE initialization to openssl makes it use hw crypto.
* Fix help text and comments on list and llist cmd.
* Output .help item=<help_item> as machine parseable data.
* Need a way to express we want a private database connection.
* Implement an unformated list type NF\_LIST
  * Use NF\_LIST e.g. non formated list for the files and basefiles of a job.
* Implement a special version of NF\_LIST e.g. RAW\_LIST
  * Use RAW\_LIST e.g. no formated raw output list for the job log of a job
* Add basic backup/restore only mode.
* Replace all bnet\_sig and bnet\_close.
* Sort the prototypes in the protos.h header.
* Added sample postgresql stored procedures for lstat field.
* Use bools instead of integers for db functions.
* Change catalog code to use a single exit from function.
* Do not use useless NULL arguments to db\_sql\_query
* Uncripple storage daemon plugins.
* Add support for newer AIX flags for acl retrieval.
* Add mount/unmount storage daemon plugin event.
* Implement baculaRegisterEvents proper.
* Allow the stored plugin to override the volume status.
* Use MAX\_BLOCK\_LENGTH instead of hardcoded 4096000
* Add clone job jobcode
* Allow open\_mail\_pipe to use extended jobcodes.
* Capture error in bpipe plugin.
* Do not define types in prototype header.
* Add additional storage plugin events.
* Align all plugin headers.
* Added support for loading plugins in btools.
* Use a generic plugin info structure for all plugins
  (fd, sd, dir) instead of a per daemon one with the
  same content.
* Only export load and unload symbols from plugins
  as "C" symbols for dynamic loading not everything.
* Added support for poll POSIX interface next to the
  normal POSIX select support. Created basic wrappers
  so we can reuse some code.
* Restored version browser in BAT, although it may be broken
  its replacement also has problems of his own. For now make
  both available and give the user something to choose something
  somewhat broken or something seriously broken.
* Possibilty to schedule Jobs on last Sunday-Saturday of the month.
* Deprecated old embeded python support in all daemons to be
  replaced with daemon plugins which interface with python.
* Increase name lengths so director/stored/filed names of more than
  30 characters can be used.
* Allow users to test plugins from a plugtest binary when developing.
* Deprecated wxwindows console.
* Deprecated old tray-monitor use qt-tray-monitor instead.
* Update config parser to allow default strings and make it more flexible.
* Warn users when they are using deprecated keywords in their config.
* Deprecated old runbeforejob/runafterjob protocol and switch to use
  new runjob protocol (introduced in 2006) for every runjob.
* Deprecated DVD support.
* Allow nextpool override from run cmdline.
* Allow nextpool override from Job definition.
* Add support for auto expanding drive selection on windows.
* Add default piddir and workingdir settings to ease configuration.
* Turn off client autopruning by default.
* Extend scheduler to allow to perform modulo scheduling.
* Implemented feature request 14 (Possibilty to schedule Jobs on last Friday of the month)
* Implemented feature request 34 (Command that releases all drives in an autochanger)
* SCSI crypto support using a storage daemon plugin
* Native NDMP backup/restore with DataManagementAgent (DMA) in director
  and NDMP Tape Server support int the storage daemon. Currently all or
  nothing restore for NDMP.
* Automatic sending of BSR (at least of the Catalog DB) vie email (#24)
* Tray-monitor shows backup activity by blinking icon

Long standing bugs fixed (bug numbers are from the bacula bug database):
* Implement feature request #1698
* Proper fix #1764 ld: warning: symbol plugin\_list has differing sizes
* Add the essentials from bugreport #1846
* Fix bug #1739 level overides ignore user input.
* Fix bug #1679 Job Overrides are Not listed In Manual Run Confirmation
* Fix bug #1965 Can not unmount with a 2-drive-library in BAT
* Fix bug #1955 bconsole inconsistent with response handling
* Fix bug #1978/#1962 Job is flagged if post-run script returns non-zero
* Fix bug #1966 bconsole does not show completions for some commands
* Fix bug #1960 qformlayout.h requires qt 4.4


Build System
============

* Fully new building system used for compiling and packaging the software
  * Based on [Open Build System](https://build.opensuse.org/)
  * Packages are build for:
      * CentOS
      * Debian
      * Fedora
      * RedHat Enterprise Linux (RHEL)
      * Suse Linux Enterprise (SLE)
      * Univention Corporate Server
      * OpenSUSE
      * Ubuntu
      * Windows VISTA and later (32 and 64 Bit)
  * Fully automatic generation of new packages within one hour after new
    release of software or bugfix.
  * Fully automatic continuos integration tests of new packages with all supported Linux distributions.
  * Windows
      * MINGW based windows Makefile set based on integration into OBS
          * Use native libraries available in OBS for MINGW32 and MINGW64
          * No longer based on archaic build system but state of the art and maintained OBS crosscompiling
      * Automatic creation of NSI installer packages
          * Supports of silent installs

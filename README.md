[BAREOS](http://www.bareos.org/) is a fork of the Bacula project.

If you want to contribute somethings to this repo please read our contribution
policy at https://www.bareos.org/en/howto-contribute.html

This source code has the following changes (highlevel) compared with
Bacula 5.2.13 (original version forked.):

Release 14.2.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=master)](https://travis-ci.org/bareos/bareos)

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
  * dh_installinit used for init scripts on debian based distributions.
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

Release 14.1.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=master)](https://travis-ci.org/bareos/bareos)

* Windows DIR.
* Support for different blocksizes (e.g. per volume blocksize) for better performance.
* Preliminary support for Cloud storage backends:
  * GFAPI (Gluster FileSystem) (tested simple backup and restore, needs more tests)
  * Object storage (S3/Swift etc.) using libdroplet (Needs patched bareos libdroplet) (Due to unusable VFS abstraction not working).
  * Rados (CEPH) (tested simple backup and restore, needs more tests)

Release 13.4.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=master)](https://travis-ci.org/bareos/bareos)

* First attempt at supporting IBM lin_tape driver.
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

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=master)](https://travis-ci.org/bareos/bareos)

* NDMP enhancements (first code for doing filelevel restores, preliminary).
* Plugin framework enhancements.
* Auto inflation/deflation storage daemon plugin (needs more testing).
* Interactive path selection refactoring.
* Add config option for storing a diagnostic device.
* Plugin framework enhancements.
* Conditionally enable VSS.
* Refactor windows version detection code.
* Implement plugin options passing.
* Upgrade all filed plugins to use fd_common.h
* Windows config files admin-only access.
* Use modern autoconf idioms.
* Enhanced help for run and restore command.
* Add command for doing a name resolution on client.
* Implement a store_alist_dir function.
* Fill the process environment with an BAREOS_CFGDIR.
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

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-13.2)](https://travis-ci.org/bareos/bareos)

* Allow to cancel multiple jobids with cancel cmd from bconsole.

Release 13.1.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=bareos-13.2)](https://travis-ci.org/bareos/bareos)

* Allow cancel by JobId on storage daemon. (#13)
* Security (filed)
   * Implementation of an allowed scriptdir keyword (#31)
   * Implementation of an allowed job command keyword (#32)
* Cleanup of compression framework (#34)
* Encryption cipher is hardcoded (#46)
* Job migration/copy between different SDs (#7)
* Use open_bpipe wrapper in bpipe plugin (#104)
   * Side effect of using open_bpipe is that you can only call
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
* Pass the restore context as parameter to display_job_parameters
* When showing the pool source show if it was set on the cmdline.
* Link bacula using libumem on Solaris the better multithreaded allocator
* Rewrite find_one_file using inline functions so its better readable.
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
* Add safeguard to append_file function for empty filepatterns.
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
  * Allow get_user_slot_list to search for specific argument.
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
* Implement an unformated list type NF_LIST
  * Use NF_LIST e.g. non formated list for the files and basefiles of a job.
* Implement a special version of NF_LIST e.g. RAW_LIST
  * Use RAW_LIST e.g. no formated raw output list for the job log of a job
* Add basic backup/restore only mode.
* Replace all bnet_sig and bnet_close.
* Sort the prototypes in the protos.h header.
* Added sample postgresql stored procedures for lstat field.
* Use bools instead of integers for db functions.
* Change catalog code to use a single exit from function.
* Do not use useless NULL arguments to db_sql_query
* Uncripple storage daemon plugins.
* Add support for newer AIX flags for acl retrieval.
* Add mount/unmount storage daemon plugin event.
* Implement baculaRegisterEvents proper.
* Allow the stored plugin to override the volume status.
* Use MAX_BLOCK_LENGTH instead of hardcoded 4096000
* Add clone job jobcode
* Allow open_mail_pipe to use extended jobcodes.
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
* Proper fix #1764 ld: warning: symbol plugin_list has differing sizes
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
      * CentOS 5
      * CentOS 6
      * Debian 6.0
      * Debian 7.0
      * Fedora 18
      * Fedora 19
      * RedHat Enterprise Linux 5 (RHEL5)
      * RedHat Enterprise Linux 6 (RHEL6)
      * Suse Linux Enterprise 11 SP1 (SLE_11_SP1)
      * Suse Linux Enterprise 11 SP2 (SLE_11_SP2)
      * Suse Linux Enterprise 11 SP3 (SLE_11_SP3)
      * Univention 3.1
      * Univention 3.2
      * OpenSuse 11.4
      * OpenSuse 12.1
      * OpenSuse 12.2
      * OpenSuse 12.3
      * OpenSuse 13.1
      * OpenSuse factory
      * Ubuntu 10.04
      * Ubuntu 12.04
      * Windows (32 and 64 Bit)
  * Fully automatic generation of new packages within one hour after new
    release of software or bugfix.
  * Fully automatic continuos integration tests of new packages with all supported Linux distributions
  * Windows
      * New clean MINGW based windows Makefile set based on integration into OBS
          * Use native libraries available in OBS for MINGW32 and MINGW64 (no more deppkgs)
          * No longer based on archaic build system but state of the art and maintained OBS crosscompiling
      * Automatic creation of NSI installer packages
          * Supports of silent installs
  * OBS uses source services to checkout current software directly from git
      * see http://en.opensuse.org/openSUSE:Build_Service_private_instance_software_live_cycle

[BAREOS](http://www.bareos.org/) is a fork of the Bacula source code with the following
changes (highlevel):

Release 12.4.x
==============

[![Build Status](https://travis-ci.org/bareos/bareos.png?branch=Branch-12.4)](https://travis-ci.org/bareos/bareos)

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
* Increase name lengths so director/stored/filed names of more then
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
      * Fedora 16
      * Fedora 17
      * Fedora 18
      * RedHat Enterprise Linux 5 (RHEL5)
      * RedHat Enterprise Linux 6 (RHEL6)
      * Suse Linux Enterprise 11 SP1 (SLE_11_SP1)
      * Suse Linux Enterprise 11 SP2 (SLE_11_SP2)
      * OpenSuse 11.4
      * OpenSuse 12.1
      * OpenSuse 12.2
      * OpenSuse factory
      * Ubuntu 12.04
  * Fully automatic generation of new packages within one hour after new
    release of software or bugfix.
  * Fully automatic continuos integration tests of new packages with all supported Linux distributions
  * Windows
      * New clean MINGW based windows Makefile set based on integration into OBS
          * Use native libraries available in OBS for MINGW32 and MINGW64 (no more deppkgs)
          * No longer based on archaic build system but state of the art and maintained OBS crosscompiling
      * Automatic creation of NSI installer packages
          * Supports of silient installs
  * OBS uses source services to checkout current software directly from git
      * see http://en.opensuse.org/openSUSE:Build_Service_private_instance_software_live_cycle

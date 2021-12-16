All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Breaking Changes

- If you are relying on the fact that Bareos doesn't try to reconnect automatically on a database drop, you now have to specify it explicitly in the Catalog configuration with a `Reconnect = no` directive. [PR #860]
- when setting an IPv6 address using the `[DIR|SD|FD] Addresses` directive, now bareos only listens on IPv6 instead of both IPv4 and IPv6 on dual-stack. If you were using the `[DIR|SD|FD] Addresses` directive to create a dual-stack socket that would listen on both IPv6 AND IPv4, it will not work that way anymore. You should now also explicitly specify the IPv4 address in the directive [PR #882]
- Support for MySQL and SQLite catalog backends has been removed [PR #949]

### Fixed
- docs: Fix links to configuration directives and issue warnings on dangling links [PR #1008]
- docs: Adapted the documentation of the VMware plugin due to update to VDDK 7 [PR #844]
- fix a bug in VMware plugin where VMDK Files were created with wrong size when using the option localvmdk=yes [PR #826]
- fix a bug where the restore browser would not recognize globbing wildcards in paths [PR #801]
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #809]
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #809]
- [Issue #1194]: when doing an accurate incremental backup, if there is a database error, a full backup is done instead of reporting the error [PR #810]
- fix a bug in a date function that leads to errors on the 31st day of a month [PR #782]
- fix possible read/write problems when using droplet with https [PR #765]
- fix "configure add" handling of quoted strings [PR #764]
- fix config-dump systemtest [PR #736]
- fix systemtests daemon control scripts [PR #762]
- fix invalid file descriptor issue in the libcloud plugin [PR #702]
- fix crash when loading both python-fd and python3-fd plugins [PR #730]
- fix parallel python plugin jobs [PR #729]
- fix oVirt plugin problem with config file [PR #729]
- [Issue #1316]: storage daemon loses a configured device instance [PR #739]
- fix python-bareos for Python < 2.7.13 [PR #748]
- fixed bug when user could enter wrong dates such as 2000-66-100 55:55:89 without being denied [PR #707]
- fix volume-pruning to be reliable on all test platforms [PR #761]
- fix memory leak in python module constants [PR #778]
- fix systemtests: reduce the number of broken tests [PR #771] [PR #791]
- [Issue #1329]: If CommandACL limits any command, no messages can be read but "you have messages" is displayed. [PR #763]
- fix gfapi-fd: avoid possible crash on second glfs_close() call [PR #792]
- docs: declare shell scripts code blocks as "sh" instead of "shell-session" [PR #802]
- [Issue #1205]: PHP 7.3 issue with compact() in HeadLink.php [PR #829]
- reorder acquire on migrate/copy to avoid possible deadlock [PR #828]
- fix drive parameter handling on big endian architectures [PR #850]
- [Issue #1324]: Infinite loop when trying to log in with invalid account [PR #840]
- [Issue #579]: Unable to connect to the director from webui via ipv6 [PR #868]
- [Issue #1300]: some job status are not categorized properly [PR #874]
- [Issue #871]: UI will not load complete [PR #880]
- [Issue #1020]: Can not restore a client with spaces in its name [PR #893]
- [Issue #971]: Error building tree for filenames with backslashes [PR #892]
- [Issue #1251]: Error when displaying pool detail [PR #903]
- [Issue #1369]: webui tries to load a nonexistent file [PR #900]
- fix lost byte in ChunkedDevice [PR #910]
- fix director crash on "update slots" when there is a parsing issue with the autochanger or tape devices [PR #919]
- [Issue #1232]: bareos logrotate errors, reintroduce su directive in logrotate [PR #918]
- fix scheduler running disabled jobs after executing the disable command [PR #924]
- [Issue #1334]: After deleting storage from the configuration, it still persists in the catalog db [PR #912]
- [Issue #1191]: The web interface runs under any login and password [PR #936]
- Adapt Python DIR and SD plugin Baseclasses to the modernized Python plugin API [PR #923]
- Fixed all compiler warnings (on our default warning level) [PR #948]
- Log LDAP info error (e.g. expired SSL cert error) [PR #956]
- Fix occassional "NULL volume name" error when non-busy, but blocked drive is unloaded [PR #973]
- Adapt percona-xtrabackup test to work on updated test environment [PR #982]
- Fix PostgreSQL create database script [PR #981]
- Unify level use with set client_min_message instruction in SQL update scripts [PR #981]
- Fixed issue with error messages not showing up properly on windows systems [PR #959]
- Fixed libdroplet xattr.h include issue by using sys/xattr.h [PR #985]
- Fixed crash on bconsole when using autcomplete with tab [PR #969]
- [Issue #1374] Include zero-file incremental backups in always-incremental consolidation [PR #995]
- fix crash in "status scheduler" command when job->client is unset [PR #965]
- [Issue #847]: fix for CVE-2017-14610 PID files that could be exploited on certain systems [PR #928]
- webui: fix a layout corner case where top navbar is hiding navtabs [PR #1022]
- webui: client details now can be displayed for client names containing dots [PR #1023]

### Added
- systemtests: make database credentials configurable [PR #950]
- systemtests: allows multiple subtests per systemtest [PR #857]
- systemtests: replaced the six reload-* tests by one test [PR #857]
- systemtests: replaced fileset-multiple-include-blocks and fileset-multiple-options-blocks by fileset-multiple-blocks test [PR #857]
- Add systemtests fileset-multiple-include-blocks, fileset-multiple-options-blocks, quota-softquota, sparse-file, truncate-command and block-size, (migrated from ``regress/``) [PR #780]
- Add bvfs and dbcheck tests to python-bareos systemtest [PR #780]
- systemtests for NDMP functionalities [PR #822]
- added choice for the drive number in the truncate command [PR #823]
- systemtests for S3 functionalities (droplet, libcloud) now use https [PR #765]
- added reload commands to systemd service [PR #694]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #723].
- added an informative debugmessage when a dynamic backend cannot be loaded [PR #740]
- support for shorter date formats, where shorter dates are compensated with lowest value possible to make a full date [PR #707]
- added external repo bareos-contrib as subtree [PR #752]
- add "copy button" to code snippets in documentation for easy copying [PR #802]
- added multicolumn prompt selection for selection of more than 20 items [PR #731]
- add script devtools/dist-tarball.sh to produce the same tarball from a cloned repo everywhere [PR #861]
- packages: Build also for openSUSE Leap 15.3 [PR #870]
- systemtest:bareos test now also runs on btrfs filesystem [PR #907]
- packages: Build also for Fedora_34 [PR #869]
- packages: Build also for Debian_11 [PR #914]
- packages: Build also for SLE_15_SP3 [PR #952]
- add job name in End Job Session output in bls tool [PR #916]
- added check for orphaned storages in dbcheck [PR #912]
- added option to delete selected storage in bconsole if it is orphaned [PR #912]
- docs: BareosSecurityIssues add remark about new systemd service (non forking) logged information into systemd-journal [PR #927]
- contrib: Add Python DIR plugin for prometheus metrics [PR #911]
- docs: added "Hardware Sizing" chapter [PR #926]
- bench: added a benchmarks tool with google-benchmark, with an initial benchmark for the restore command [PR #883]
- database: Add index on jobmedia mediaid,jobid [PR #958]
- contrib: Add Python FD Plugin for OpenVZ container [PR #908]
- packages: Build also for Fedora_35 [PR #972]
- cmake: check for chflags() function and enable FreeBSD File Flags support [PR #963]
- plugin: added mariabackup python plugin, added systemtest for mariabackup and updated systemtest for percona-xtrabackup [PR #967]
- webui: add inchanger column to volume tables [PR #998]
- webui and console: add job duration column in job listings [PR #1007]
- packages: Build EL_8 packages instead of CentOS_8 for RHEL 8 derivates (Alma, Rocky, CentOS Stream)[PR #992]
- webui: provide a notification that NDMP restores are NOT supported by webui [PR #1020]
- console: prune command gained support to prune multiple volumes at once [PR #966]

### Changed
- docs: check if configuration directives are defined as CamelCase in core. Otherwise building the documentation will fail with an error [PR #1008]
- docs: Improve data-type subsubsection in CustomizingTheConfiguration. Add a special Sphinx directive for them (``config:datatype``) [PR #1008]
- systemtest python-bareos: split tests in separate files [PR #944]
- core: systemd service: change daemon type from forking to simple and start daemons in foreground [PR #824]
- systemtests: define variable BackupDirectory globally [PR #780]
- systemtests: run all systemstests with ``set -o pipefail`` [PR #780]
- core: cleanup systemd service dependencies: Requires network.target, but start after the network-online.target [PR #700]
- core: Make the jansson library mandatory when compiling the Bareos Director [PR #793]
- repaired or added all header guards in libdroplet [PR #765]
- When using Python > 3.7 the postgres and libcloud plugins will cancel the job and write an error message [PR #769]
- bstrncpy: workaround when used with overlapping strings [PR #736]
- Disabled test "statefile" for big endian, use temporary state files for all other architectures [PR #757]
- Fixed broken link in https://docs.bareos.org/IntroductionAndTutorial/WhatIsBareos.html documentation page
- Package **bareos-database-postgresql**: add recommendation for package **dbconfig-pgsql**.
- Adapt the init scripts for some platform to not refer to a specific (outdated) configuration file, but to use the default config file instead.
- scripts: cleaned up code for postgresql db creation [PR #709]
- Change Copy Job behaviour regarding Archive Jobs [PR #717]
- py2lug-fd-ovirt systemtest: use ovirt-plugin.ini config file [PR #729]
- Ctest now runs in scripted mode. [PR #742]
- storage daemon: class Device: rename dev_name to archive_device_string (as the value stored here is the value of the "Archive Device" directive) [PR #744]
- Enable c++17 support [PR #741]
- webui: Localization updated [PR #776]
- running cmake for the core-directory only is now forbidden [PR #767]
- dird: ignore duplicate job checking on virtual fulls started by consolidation [PR #552]
- buildsystem: switch to cross build chain of Fedora 34 [PR #819]
- FreeBSD: adapt pkglists for FreeBSD 13.0 [PR #819]
- Fedora34: do not build mysql db backend, adapt pkglist [PR #819]
- bscan and bareos systemtests: also test bextract and bls binaries, use autoxflate plugin and FSType fileset options [PR #790]
- Windows release package now ships source code as optional component, so there is no need for a debug-package anymore [PR #858]
- postgresql filedaemon plugin: switched from psycopg2 to pg8000, dropped support for python2.
- version information was moved from core/cmake/ and webui/cmake/ directories into the toplevel cmake/ directory [PR #861]
- add chromedriver options to improve reliability of selenium tests [PR #920]
- docs: Describe how to get debugging info when using the VMware plugin [PR #921]
- reconnecting to the database is now automatic per default without the need to specify it in the catalog [PR #860]
- bareos is now set to listen to both IPv6 and IPv4 by default, instead of needing to specify it via a directive [PR #882]
- bareos is now able to create IPv6 addresses with the `DirAddress` directive [PR #882]
- Disable autotosectionlabel plugin in documentation and add required labels [PR #942]
- webui: improve log message keyword highlighting [PR #937]
- webui: change retention/expiration column to only show retention on volume tables [PR #943]
- webui: introduce confirmation dialog on rerun and cancel job actions [PR #951]
- docs: update documentation of config data types [PR #962]
- Allow `release` command to be used in scripts [PR #961]
- Show more details about subscriptions and units data with `status subscriptions` [PR #989]
- webui: format total jobs and total files language-sensitive [PR #996]
- webui: optimize bvfs update cache calls [PR #999]
- Replace complicated memory pool allocator with plain malloc() [PR #997]
- webui: update localization [PR #1018]

### Deprecated
- Deprecated directives [PR #928]
  - Director
    - Director resource
      - `Pid Directory`
  - Storage Daemon
    - Storage resource
      - `Pid Directory`
  - File Daemon
    - Client resource
      - `Pid Directory`

### Removed
- Remove regression tests (``regress/`` directory). Tests still relevant tests have been migrated into systemtests [PR #780]
- Removed outdated configuration files (example files).
- Removed package **bareos-devel**.
- Removed package **bareos-regress** and **bareos-regress-config**. The package **bareos-regress** has not been build for a long time.
- Removed unused script **breload**.
- Removed some workaround for Ubuntu 8.04.
- Removed outdated support for platforms Alpha, BSDi, Gentoo, Irix and Mandrake.
- Removed language support files for the core daemons, as these files are outdated and not used at all.
- Removed package lists for platforms no longer built: Fedora_30.x86_64, RHEL_6.x86_64, SLE_12_SP4.x86_64, openSUSE_Leap_15.0.x86_64, openSUSE_Leap_15.1.x86_64.
- Removed support for IPv4-only build. IPv6 now is required for building bareos. [PR #913]
- Remove MySQL and SQLite catalog backends [PR #949]
- Remove deprecated directives [PR #938]
  - Director
    - Director resource
      - `Omit Defaults`
      - `Sub Sys Directory`
    - Storage resource
      - `Sdd Port`
    - Pool resource
      - `Use Volume Once`
    - Job resource
      - `Write Part After Job`
      - Alias `Differential Max Wait Time` for `Differential Max Runtime`
      - Alias `Full Max Wait Time` for `Full Max Runtime`
      - Alias `Incremental Max Wait Time` for `Incremental Max Runtime`
      - Alias `Plugin Options` for `Fd Plugin Options`
    - Client resource
      - Alias `Allow Client Connect` for `Connection From Client To Director`
  - Storage Daemon
    - Device resource
      - `Free Space Command`
      - `Maximum Part Size`
      - `Write Part Command`
      - `Maximum Volume Size`
    - Storage resource
      - `Sub Sys Directory`
  - File Daemon
    - Client resource
      - `Sub Sys Directory`

### Security
- PID files are now created before dropping privileges and before the parent process exits. [PR #928]

### Documentation
- Restore error "could not hard link" documented: what is the cause and how it can be avoided or solved. [PR #759]
- Developer guide: add small chapter about c++ exceptions. [PR #777]

[Issue #579]: https://bugs.bareos.org/view.php?id=579
[Issue #847]: https://bugs.bareos.org/view.php?id=847
[Issue #871]: https://bugs.bareos.org/view.php?id=871
[Issue #971]: https://bugs.bareos.org/view.php?id=971
[Issue #1020]: https://bugs.bareos.org/view.php?id=1020
[Issue #1191]: https://bugs.bareos.org/view.php?id=1191
[Issue #1194]: https://bugs.bareos.org/view.php?id=1194
[Issue #1205]: https://bugs.bareos.org/view.php?id=1205
[Issue #1232]: https://bugs.bareos.org/view.php?id=1232
[Issue #1251]: https://bugs.bareos.org/view.php?id=1251
[Issue #1300]: https://bugs.bareos.org/view.php?id=1300
[Issue #1316]: https://bugs.bareos.org/view.php?id=1316
[Issue #1324]: https://bugs.bareos.org/view.php?id=1324
[Issue #1329]: https://bugs.bareos.org/view.php?id=1329
[Issue #1334]: https://bugs.bareos.org/view.php?id=1334
[Issue #1369]: https://bugs.bareos.org/view.php?id=1369
[Issue #1374]: https://bugs.bareos.org/view.php?id=1374
[PR #552]: https://github.com/bareos/bareos/pull/552
[PR #694]: https://github.com/bareos/bareos/pull/694
[PR #700]: https://github.com/bareos/bareos/pull/700
[PR #702]: https://github.com/bareos/bareos/pull/702
[PR #707]: https://github.com/bareos/bareos/pull/707
[PR #709]: https://github.com/bareos/bareos/pull/709
[PR #717]: https://github.com/bareos/bareos/pull/717
[PR #723]: https://github.com/bareos/bareos/pull/723
[PR #729]: https://github.com/bareos/bareos/pull/729
[PR #730]: https://github.com/bareos/bareos/pull/730
[PR #731]: https://github.com/bareos/bareos/pull/731
[PR #736]: https://github.com/bareos/bareos/pull/736
[PR #739]: https://github.com/bareos/bareos/pull/739
[PR #740]: https://github.com/bareos/bareos/pull/740
[PR #741]: https://github.com/bareos/bareos/pull/741
[PR #742]: https://github.com/bareos/bareos/pull/742
[PR #744]: https://github.com/bareos/bareos/pull/744
[PR #748]: https://github.com/bareos/bareos/pull/748
[PR #752]: https://github.com/bareos/bareos/pull/752
[PR #757]: https://github.com/bareos/bareos/pull/757
[PR #759]: https://github.com/bareos/bareos/pull/759
[PR #761]: https://github.com/bareos/bareos/pull/761
[PR #762]: https://github.com/bareos/bareos/pull/762
[PR #763]: https://github.com/bareos/bareos/pull/763
[PR #764]: https://github.com/bareos/bareos/pull/764
[PR #765]: https://github.com/bareos/bareos/pull/765
[PR #767]: https://github.com/bareos/bareos/pull/767
[PR #769]: https://github.com/bareos/bareos/pull/769
[PR #771]: https://github.com/bareos/bareos/pull/771
[PR #776]: https://github.com/bareos/bareos/pull/776
[PR #777]: https://github.com/bareos/bareos/pull/777
[PR #778]: https://github.com/bareos/bareos/pull/778
[PR #780]: https://github.com/bareos/bareos/pull/780
[PR #782]: https://github.com/bareos/bareos/pull/782
[PR #790]: https://github.com/bareos/bareos/pull/790
[PR #791]: https://github.com/bareos/bareos/pull/791
[PR #792]: https://github.com/bareos/bareos/pull/792
[PR #793]: https://github.com/bareos/bareos/pull/793
[PR #801]: https://github.com/bareos/bareos/pull/801
[PR #802]: https://github.com/bareos/bareos/pull/802
[PR #809]: https://github.com/bareos/bareos/pull/809
[PR #810]: https://github.com/bareos/bareos/pull/810
[PR #819]: https://github.com/bareos/bareos/pull/819
[PR #822]: https://github.com/bareos/bareos/pull/822
[PR #823]: https://github.com/bareos/bareos/pull/823
[PR #824]: https://github.com/bareos/bareos/pull/824
[PR #826]: https://github.com/bareos/bareos/pull/826
[PR #828]: https://github.com/bareos/bareos/pull/828
[PR #829]: https://github.com/bareos/bareos/pull/829
[PR #840]: https://github.com/bareos/bareos/pull/840
[PR #844]: https://github.com/bareos/bareos/pull/844
[PR #850]: https://github.com/bareos/bareos/pull/850
[PR #857]: https://github.com/bareos/bareos/pull/857
[PR #858]: https://github.com/bareos/bareos/pull/858
[PR #860]: https://github.com/bareos/bareos/pull/860
[PR #861]: https://github.com/bareos/bareos/pull/861
[PR #868]: https://github.com/bareos/bareos/pull/868
[PR #869]: https://github.com/bareos/bareos/pull/869
[PR #870]: https://github.com/bareos/bareos/pull/870
[PR #874]: https://github.com/bareos/bareos/pull/874
[PR #880]: https://github.com/bareos/bareos/pull/880
[PR #882]: https://github.com/bareos/bareos/pull/882
[PR #883]: https://github.com/bareos/bareos/pull/883
[PR #892]: https://github.com/bareos/bareos/pull/892
[PR #893]: https://github.com/bareos/bareos/pull/893
[PR #900]: https://github.com/bareos/bareos/pull/900
[PR #903]: https://github.com/bareos/bareos/pull/903
[PR #907]: https://github.com/bareos/bareos/pull/907
[PR #908]: https://github.com/bareos/bareos/pull/908
[PR #910]: https://github.com/bareos/bareos/pull/910
[PR #911]: https://github.com/bareos/bareos/pull/911
[PR #912]: https://github.com/bareos/bareos/pull/912
[PR #913]: https://github.com/bareos/bareos/pull/913
[PR #914]: https://github.com/bareos/bareos/pull/914
[PR #916]: https://github.com/bareos/bareos/pull/916
[PR #918]: https://github.com/bareos/bareos/pull/918
[PR #919]: https://github.com/bareos/bareos/pull/919
[PR #920]: https://github.com/bareos/bareos/pull/920
[PR #921]: https://github.com/bareos/bareos/pull/921
[PR #923]: https://github.com/bareos/bareos/pull/923
[PR #924]: https://github.com/bareos/bareos/pull/924
[PR #926]: https://github.com/bareos/bareos/pull/926
[PR #927]: https://github.com/bareos/bareos/pull/927
[PR #928]: https://github.com/bareos/bareos/pull/928
[PR #936]: https://github.com/bareos/bareos/pull/936
[PR #937]: https://github.com/bareos/bareos/pull/937
[PR #938]: https://github.com/bareos/bareos/pull/938
[PR #942]: https://github.com/bareos/bareos/pull/942
[PR #943]: https://github.com/bareos/bareos/pull/943
[PR #944]: https://github.com/bareos/bareos/pull/944
[PR #948]: https://github.com/bareos/bareos/pull/948
[PR #949]: https://github.com/bareos/bareos/pull/949
[PR #950]: https://github.com/bareos/bareos/pull/950
[PR #951]: https://github.com/bareos/bareos/pull/951
[PR #952]: https://github.com/bareos/bareos/pull/952
[PR #956]: https://github.com/bareos/bareos/pull/956
[PR #958]: https://github.com/bareos/bareos/pull/958
[PR #959]: https://github.com/bareos/bareos/pull/959
[PR #961]: https://github.com/bareos/bareos/pull/961
[PR #962]: https://github.com/bareos/bareos/pull/962
[PR #963]: https://github.com/bareos/bareos/pull/963
[PR #965]: https://github.com/bareos/bareos/pull/965
[PR #966]: https://github.com/bareos/bareos/pull/966
[PR #967]: https://github.com/bareos/bareos/pull/967
[PR #969]: https://github.com/bareos/bareos/pull/969
[PR #972]: https://github.com/bareos/bareos/pull/972
[PR #973]: https://github.com/bareos/bareos/pull/973
[PR #981]: https://github.com/bareos/bareos/pull/981
[PR #982]: https://github.com/bareos/bareos/pull/982
[PR #985]: https://github.com/bareos/bareos/pull/985
[PR #989]: https://github.com/bareos/bareos/pull/989
[PR #992]: https://github.com/bareos/bareos/pull/992
[PR #995]: https://github.com/bareos/bareos/pull/995
[PR #996]: https://github.com/bareos/bareos/pull/996
[PR #997]: https://github.com/bareos/bareos/pull/997
[PR #998]: https://github.com/bareos/bareos/pull/998
[PR #999]: https://github.com/bareos/bareos/pull/999
[PR #1007]: https://github.com/bareos/bareos/pull/1007
[PR #1008]: https://github.com/bareos/bareos/pull/1008
[PR #1018]: https://github.com/bareos/bareos/pull/1018
[PR #1020]: https://github.com/bareos/bareos/pull/1020
[PR #1022]: https://github.com/bareos/bareos/pull/1022
[unreleased]: https://github.com/bareos/bareos/tree/master

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- build: switch from FreeBSD 13.0 to 13.1 [PR #1256]

### Fixed
- dird: RunScript fixes [PR #1222]
  - fix show command output for RunScript RunsOnClient
  - fix show verbose for RunScripts
  - execute console runscripts only on the Director
- devtools/dist-tarball.sh: fix name if version contains "~pre" [PR #1220]

### Documentation
- Univention Corporate Server (UCS) >= 5: same support as for other Linux distributions, but no extended integration as for UCS < 5 [PR #1245]

- backport mssql add a warning in case of pitr to run another backup full or diff afterwards [PR #1246]
- backport improve troubleshooting and debugging chapter [PR #1246]
- backport update Appendix/HardwareSizing DB size with new numbers and formulas [BUG #1477][PR #1246]
- backport add description to fileset signature sha256 and sha512 parameter [PR #1246]
- backport add example to selection type = sql query [PR #1246]
- backport improve Wild(*) section with links to example [PR #1246]
- backport FreeBSD install create /usr/local/etc/pkg/repos directory [PR #1246]
- backport dir-job-RescheduleTimes remove wrong default sentence [PR #1246]

## [21.1.4] - 2022-08-05

### Fixed
- debian: fix package dependencies for webui and Ceph [PR #1184]

### Added
- build: Add support for Ubuntu 22.04, Fedora 36, EL 9, openSUSE 15.4 [PR #1191]
- build: Add support for SLE_15_SP4 [PR #1207]

### Changed
- doc: clarifies CheckFileChanges option not intended to be used with plugin [BUG #1452][PR #1195]
- config parser: Refactor config reloading by using shared pointers instead of callbacks [PR #1214]

### Security

### Documentation
- doc: backport FIPS section in security [PR #1212]


## [21.1.3] - 2022-05-09

### Fixed
- webui: get volume and pool params from query instead of route [PR #1144]
- FreeBSD packages: add missing ddl/update 2171_2192 and 2192_2210 files [PR #1148]
- [Issue #1445] adding quotes to director name when using `configure export`[PR #1173]
- dir: miscalculation when using `always incremental keep number` [PR #1160]

### Changed
- cats: include only jobtypes in list jobtotals that write data to volumes [PR #1136]
- jstreegrid: remove handling of IE < 8 using navigator interface to avoid warnings in chrome [PR #1141]
- `bvfs_update` now uses `unordered_map` instead of `htable` for the pathid cache [PR #1146]
- Qmsg: in case of syslog logging use adapted log priority instead of always LOG_ERR [PR #1150]

### Security
- webui: update moment.js to version 2.29.2 [PR #1156] [CVE-2022-24785](https://github.com/advisories/GHSA-8hfj-j24r-96c4)
  - webui is NOT affected
  - webui does NOT use the npm server
  - webui does NOT use a user-provided locale string to directly switch moment locale

### Documentation
- backport fix incorrect link in contrib PythonFdPlugin [Issue #1450] [PR #1167]

## [21.1.2] - 2022-03-17

### Fixed
- dir: fix crash when there are no jobs to consolidate [PR #1132]

## [21.1.1] - 2022-03-16

### Fixed
- webui: fix restore file tree rendering [PR #1128]

## [21.1.0] - 2022-03-15

### Breaking Changes
- when using PAM Bareos will now check authorization, too. If authorization is not configured, login will fail. See [updated documentation](https://docs.bareos.org/TasksAndConcepts/PAM.html#configuration) on how to proceed [PR #1118].

### Fixed
- NDMP_BAREOS: support autoxflate plugin [PR #1090] (backport of [PR #1013])
- debian: add missing python plugin dependencies [PR #1045]
- fix empty job timeline issue if date.timezone is not set in php.ini [PR #1053] (backport of [PR #1051])
- Fix context confusion in Director's Python plugins [PR #1076]
- webui: fix deprecated notice required param follows optional param [PR #1100]
- webui: fix undefined array key warning [PR #1102]
- core cats: Remove PGSQLVERSION if block in update_bareos_tables to support all PG versions [PR #1075] (part backport of [PR #1073])
- webui: fix uncaught TypeError if node.data is null [PR #1096]
- core cats: Add DROP VIEWS instructions in drop_bareos_table script [PR #1095]
- webui: show DIR message if ACL prevents a job rerun [PR #1111]

### Added
- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1049]
- packaging: installation and upgrade will check for the presence of :file:`.enable-cap_sys_rawio` in your bareos config dir and will configure the required cap_sys_rawio capabilities [PR #1066] backport from original pull request [PR #1057]
- webui: add timeline chart by jobs [PR #1063]
- webui: analytics module: show stored data per jobname in treemap [PR #1116]
- webui: add pool column to volume lists [PR #1125]
- cats: make `.bvfs_update` and `.bvfs_versions` take archive jobs into consideration [PR #1164]

### Changed
- webui: add timeline chart by jobs [PR #1063]
- Consolidation now purges candidate jobs with no files instead of ignoring them [PR #1107]
- Virtual Full will now terminate if one if the input jobs had its files pruned [PR #1108]
- Don't keep volume open after acquiring a read-storage failed in migrate/copy/virtual full  [PR #1113]
- webui: remove an unnecessary .bvfs_get_jobids and buildSubtree() call [PR #1050]
- git: set merge strategy for CHANGELOG.md to union [PR #1062]
- webui: add timeline chart by jobs [PR #1059]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1021] 
- dir, stored: start statistics threads only if needed [PR #1040]
- gitignore: cleanup .gitignore files [PR #1067]
- webui: update jstree from v3.3.8 to v3.3.12 [PR #1088]
- webui: update jstree-grid plugin [PR #1089]
- Consolidation now purges candidate jobs with no files instead of ignoring them [PR #1056]
- Virtual Full will now terminate if one if the input jobs had its files pruned [PR #1070]
- webui: new login screen background and adapted logo to support Ukraine  [PR #1117]
- console: multicolumn output: fill columns first [PR #1133]
- cats: filtered zero file jobs list is now sorted [PR #1175]

### Security
- webui: update jquery from v3.2.0 to v3.6.0 [PR #1084]
- dird: check authorization on PAM login [PR #1118] [CVE-2022-24755](https://github.com/bareos/bareos/security/advisories/GHSA-4979-8ffj-4q26)
- dird: fix memory leak on failed PAM login [PR #1118] [CVE-2022-24756](https://github.com/bareos/bareos/security/advisories/GHSA-jh55-4wgw-xc9j)

### Documentation
- backport chapter for mariabackup db plugin [PR #1044]
- backport python action plugin example improvement [PR #1080] (backport of [PR #1079])
- backport howto improve dbcopy migration [PR #1094]

## [21.0.0] - 2021-12-21

* 300+ PRs
* 1.000+ commits since Bareos 20
* 30 contributors
* 72.170 line added
* 318.812 lines removed

### Breaking Changes
- Bareos now automatically reconnects to the catalog database after the connection was lost. To keep the old behaviour, set `Reconnect = no`  in your Catalog resource. [PR #860]
- Bareos now listens on IPv4 and IPv6 by default. If you currently use `[DIR|SD|FD] Address` or `[DIR|SD|FD] Address`, you may need to update your configuration to get the same behaviour as before. [PR #882]
- Support for MySQL and SQLite catalog backends has been removed. Switch to PostgreSQL via `bareos-dbcopy` on Bareos 20 before upgrading. See https://docs.bareos.org/Appendix/Howtos.html#section-migrationmysqltopostgresql [PR #949]
- Bareos no longer supports Bacula tape formats <= 10 (Bacula <= 1.26 which was released 2002). [PR #1019]
- Deprecated configuration directives have been removed. If you have any configuration settings deprecated in Bareos 20, you will need to remove these before upgrading. [PR #938]

### Fixed
- fix wrong `packages_dir` in restapi workflow, so restapi packages will be released to PyPI [PR #1033]
- debian: Let dbconfig create the Bareos catalog also with LC_COLLATE='C' and LC_CTYPE='C'. The create_bareos_database script did always do so. Requires dbconfig >= 2.0.21 [PR #1031]
- [Issue #1374] Include zero-file incremental backups in always-incremental consolidation [PR #995]
- [Issue #847]: fix for [CVE-2017-14610](https://github.com/bareos/bareos/security/advisories/GHSA-426r-vgh8-vrw8) PID files that could be exploited on certain systems [PR #928]
- [Issue #1194]: when doing an accurate incremental backup, if there is a database error, a full backup is done instead of reporting the error [PR #810]
- fix a bug in VMware plugin where VMDK Files were created with wrong size when using the option `localvmdk=yes` [PR #826]
- fix a bug where the restore browser would not recognize globbing wildcards in paths [PR #801]
- [Issue #1329]: If CommandACL limits any command, no messages can be read but "you have messages" is displayed. [PR #763]
- fix lost byte in ChunkedDevice [PR #910]
- fix director crash on "update slots" when there is a parsing issue with the autochanger or tape devices [PR #919]
- Fix occassional "NULL volume name" error when non-busy, but blocked drive is unloaded [PR #973]
- reorder acquire on migrate/copy to avoid possible deadlock [PR #828]
- fix scheduler running disabled jobs after executing the disable command [PR #924]
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #809]
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #809]
- fix possible read/write problems when using droplet with https [PR #765]
- fix "configure add" handling of quoted strings [PR #764]
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
- fix gfapi-fd: avoid possible crash on second glfs_close() call [PR #792]
- [Issue #1205]: PHP 7.3 issue with compact() in HeadLink.php [PR #829]
- fix drive parameter handling on big endian architectures [PR #850]
- [Issue #1324]: Infinite loop when trying to log in with invalid account [PR #840]
- [Issue #579]: Unable to connect to the director from webui via ipv6 [PR #868]
- [Issue #1300]: some job status are not categorized properly [PR #874]
- [Issue #871]: UI will not load complete [PR #880]
- [Issue #1020]: Can not restore a client with spaces in its name [PR #893]
- [Issue #971]: Error building tree for filenames with backslashes [PR #892]
- [Issue #1251]: Error when displaying pool detail [PR #903]
- [Issue #1369]: webui tries to load a nonexistent file [PR #900]
- [Issue #1232]: bareos logrotate errors, reintroduce su directive in logrotate [PR #918]
- [Issue #1334]: After deleting storage from the configuration, it still persists in the catalog db [PR #912]
- [Issue #1191]: The web interface runs under any login and password [PR #936]
- Adapt Python DIR and SD plugin Baseclasses to the modernized Python plugin API [PR #923]
- Fixed all compiler warnings (on our default warning level) [PR #948]
- Log LDAP info error (e.g. expired SSL cert error) [PR #956]
- Adapt percona-xtrabackup test to work on updated test environment [PR #982]
- fix crash in "status scheduler" command when job->client is unset [PR #965]
- webui: fix a layout corner case where top navbar is hiding navtabs [PR #1022]
- webui: client details now can be displayed for client names containing dots [PR #1023]
- Fixed issue with error messages not showing up properly on windows systems [PR #959]
- Fixed libdroplet xattr.h include issue by using sys/xattr.h [PR #985]
- Fixed crash on bconsole when using autcomplete with tab [PR #969]
- Fix PostgreSQL create database script [PR #981]
- Unify level use with set client_min_message instruction in SQL update scripts [PR #981]
- dird: avoid crash in job listing when "current" and "count" are both specified at the same time [PR #1026]
- Fixed issue with `llist jobs last current enable` that would not display correct data [PR #1025]
- fix a bug in a date function that leads to errors on the 31st day of a month [PR #782]
- fix config-dump systemtest [PR #736]
- fix systemtests daemon control scripts [PR #762]
- SQL: queries: fix sql queries to handle negative job duration value [PR #1201]
- dird: fix TLS-PSK credential not found error with very long job names [PR #1208]
- dird: fix odd-even weeks parsing bug in schedule [PR #1232]

### Added
- plugin: added mariabackup python plugin, added systemtest for mariabackup and updated systemtest for percona-xtrabackup [PR #967]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #723].
- add `reload` commands to systemd service [PR #694]
- add multicolumn prompt selection for selection of more than 20 items [PR #731]
- add choice for the drive number in the truncate command [PR #823]
- support for shorter date formats, where shorter dates are compensated with lowest value possible to make a full date [PR #707]
- added option to delete selected storage in bconsole if it is orphaned [PR #912]
- webui: add inchanger column to volume tables [PR #998]
- webui and console: add job duration column in job listings [PR #1007]
- console: prune command gained support to prune multiple volumes at once [PR #966]
- webui: introduce a job timeline chart [PR #1017]
- add support for Universal Linux Client [PR #1019]
- systemtests: make database credentials configurable [PR #950]
- systemtests: allows multiple subtests per systemtest [PR #857]
- systemtests: replaced the six reload-* tests by one test [PR #857]
- systemtests: replaced fileset-multiple-include-blocks and fileset-multiple-options-blocks by fileset-multiple-blocks test [PR #857]
- systemtests: add systemtests fileset-multiple-include-blocks, fileset-multiple-options-blocks, quota-softquota, sparse-file, truncate-command and block-size, (migrated from ``regress/``) [PR #780]
- add bvfs and dbcheck tests to python-bareos systemtest [PR #780]
- systemtests for NDMP functionalities [PR #822]
- systemtests for S3 functionalities (droplet, libcloud) now use https [PR #765]
- add an informative debugmessage when a dynamic backend cannot be loaded [PR #740]
- add external repo bareos-contrib as subtree [PR #752]
- add script devtools/dist-tarball.sh to produce the same tarball from a cloned repo everywhere [PR #861]
- packages: Build also for openSUSE Leap 15.3 [PR #870]
- systemtest:bareos test now also runs on btrfs filesystem [PR #907]
- packages: Build also for Fedora_34 [PR #869]
- packages: Build also for Debian_11 [PR #914]
- packages: Build also for SLE_15_SP3 [PR #952]
- add job name in End Job Session output in bls tool [PR #916]
- added check for orphaned storages in dbcheck [PR #912]
- contrib: Add Python DIR plugin for prometheus metrics [PR #911]
- bench: added a benchmarks tool with google-benchmark, with an initial benchmark for the restore command [PR #883]
- database: Add index on jobmedia mediaid,jobid [PR #958]
- contrib: Add Python FD Plugin for OpenVZ container [PR #908]
- packages: Build also for Fedora_35 [PR #972]
- cmake: check for chflags() function and enable FreeBSD File Flags support [PR #963]
- packages: Build EL_8 packages instead of CentOS_8 for RHEL 8 derivates (Alma, Rocky, CentOS Stream)[PR #992]
- webui: provide a notification that NDMP restores are NOT supported by webui [PR #1020]
- systemtests: add a test for the jobs last status table in webui [PR #1024]

### Changed
- core: systemd service: change daemon type from forking to simple and start daemons in foreground [PR #824]
- debian: unify daemon (Dir,SD,FD) start behavior with RPM based distributions. Don't start any daemon after installation. Enable all daemons. Only restart FD after upgrade (before: FD has been started after installation, No daemon have been restarted on upgrade) [PR #1019] and [PR #1029]
- core: cleanup systemd service dependencies: Requires network.target, but start after the network-online.target [PR #700]
- Enable c++17 support [PR #741]
- postgresql filedaemon plugin: switched from psycopg2 to pg8000, dropped support for python2.
- reconnecting to the database is now automatic per default without the need to specify it in the catalog [PR #860]
- bareos is now set to listen to both IPv6 and IPv4 by default, instead of needing to specify it via a directive [PR #882]
- bareos is now able to create IPv6 addresses with the `DirAddress` directive [PR #882]
- show more details about subscriptions and units data with `status subscriptions` [PR #989]
- webui: optimize bvfs update cache calls [PR #999]
- replace complicated memory pool allocator with plain malloc() [PR #997]
- systemtest python-bareos: split tests in separate files [PR #944]
- systemtests: define variable BackupDirectory globally [PR #780]
- systemtests: run all systemstests with ``set -o pipefail`` [PR #780]
- core: Make the jansson library mandatory when compiling the Bareos Director [PR #793]
- repaired or added all header guards in libdroplet [PR #765]
- When using Python > 3.7 the postgres and libcloud plugins will cancel the job and write an error message [PR #769]
- bstrncpy: workaround when used with overlapping strings [PR #736]
- Disabled test "statefile" for big endian, use temporary state files for all other architectures [PR #757]
- Package **bareos-database-postgresql**: add recommendation for package **dbconfig-pgsql**.
- Adapt the init scripts for some platform to not refer to a specific (outdated) configuration file, but to use the default config file instead.
- scripts: cleaned up code for postgresql db creation [PR #709]
- Change Copy Job behaviour regarding Archive Jobs [PR #717]
- py2lug-fd-ovirt systemtest: use ovirt-plugin.ini config file [PR #729]
- Ctest now runs in scripted mode. [PR #742]
- storage daemon: class Device: rename dev_name to archive_device_string (as the value stored here is the value of the "Archive Device" directive) [PR #744]
- webui: Localization updated [PR #776]
- running cmake for the core-directory only is now forbidden [PR #767]
- dird: ignore duplicate job checking on virtual fulls started by consolidation [PR #552]
- buildsystem: switch to cross build chain of Fedora 34 [PR #819]
- FreeBSD: adapt pkglists for FreeBSD 13.0 [PR #819]
- Fedora34: do not build mysql db backend, adapt pkglist [PR #819]
- bscan and bareos systemtests: also test bextract and bls binaries, use autoxflate plugin and FSType fileset options [PR #790]
- Windows release package now ships source code as optional component, so there is no need for a debug-package anymore [PR #858]
- version information was moved from core/cmake/ and webui/cmake/ directories into the toplevel cmake/ directory [PR #861]
- add chromedriver options to improve reliability of selenium tests [PR #920]
- webui: improve log message keyword highlighting [PR #937]
- webui: change retention/expiration column to only show retention on volume tables [PR #943]
- webui: introduce confirmation dialog on rerun and cancel job actions [PR #951]
- Allow `release` command to be used in scripts [PR #961]
- webui: format total jobs and total files language-sensitive [PR #996]
- webui: update localization [PR #1018]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1065] (backport of [PR #1021])

### Deprecated
- Deprecated directives [PR #928] and [PR #949]
  - Director
    - Director resource
      - `Pid Directory`
    - Catalog resource
      - `Db Driver`
  - Storage Daemon
    - Storage resource
      - `Pid Directory`
  - File Daemon
    - Client resource
      - `Pid Directory`

### Removed
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
- Debian: Old SysV init scripts have been removed. Bareos daemons are controlled by systemd [PR #1019]
- Removed language support files for the core daemons, as these files are outdated and not used at all.
- core: remove deprecated config directive `dbdriver` from all config files, templates and from automatic configuration during install [PR #1027]
- Remove regression tests (``regress/`` directory). Tests still relevant tests have been migrated into systemtests [PR #780]
- Removed support for IPv4-only build. IPv6 now is required for building bareos. [PR #913]
- Removed package lists for platforms no longer built: Fedora_30.x86_64, RHEL_6.x86_64, SLE_12_SP4.x86_64, openSUSE_Leap_15.0.x86_64, openSUSE_Leap_15.1.x86_64.
- Removed outdated support for platforms Alpha, BSDi, Gentoo, Irix and Mandrake.
- Removed outdated configuration files (example files).
- Removed package **bareos-devel**.
- Removed package **bareos-regress** and **bareos-regress-config**. The package **bareos-regress** has not been build for a long time.
- Removed unused script **breload**.
- Removed some workaround for Ubuntu 8.04.

### Security
- [CVE-2017-14610](https://github.com/bareos/bareos/security/advisories/GHSA-426r-vgh8-vrw8): PID files are now created before dropping privileges and before the parent process exits. [PR #928]

### Documentation
- cleanup update section [PR #1055]
- clarifies MySQL catalog migration process [PR #1055]
- split `Howtos.rst` file into one file per section [PR #1055]
- split the very long `Plugins.rst` file into one file per Bareos plugin [PR #1052]
- add documentation about Unified Linux Client [PR #1028]
- added "Hardware Sizing" chapter [PR #926]
- Improve data-type subsubsection in CustomizingTheConfiguration. Add a special Sphinx directive for them (``config:datatype``) [PR #1008]
- add "copy button" to code snippets in documentation for easy copying [PR #802]
- Restore error "could not hard link" documented: what is the cause and how it can be avoided or solved. [PR #759]
- Developer guide: add small chapter about c++ exceptions. [PR #777]
- Fix links to configuration directives and issue warnings on dangling links [PR #1008]
- Adapted the documentation of the VMware plugin due to update to VDDK 7 [PR #844]
- declare shell scripts code blocks as "sh" instead of "shell-session" [PR #802]
- BareosSecurityIssues add remark about new systemd service (non forking) logged information into systemd-journal [PR #927]
- check if configuration directives are defined as CamelCase in core. Otherwise building the documentation will fail with an error [PR #1008]
- Describe how to get debugging info when using the VMware plugin [PR #921]
- Disable autotosectionlabel plugin in documentation and add required labels [PR #942]
- update documentation of config data types [PR #962]
- Fixed broken link in https://docs.bareos.org/IntroductionAndTutorial/WhatIsBareos.html documentation page


[21.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F21.0.0
[21.1.0]: https://github.com/bareos/bareos/releases/tag/Release%2F21.1.0
[21.1.1]: https://github.com/bareos/bareos/releases/tag/Release%2F21.1.1
[21.1.2]: https://github.com/bareos/bareos/releases/tag/Release%2F21.1.2
[21.1.3]: https://github.com/bareos/bareos/releases/tag/Release%2F21.1.3
[21.1.4]: https://github.com/bareos/bareos/releases/tag/Release%2F21.1.4
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
[Issue #1445]: https://bugs.bareos.org/view.php?id=1445
[Issue #1450]: https://bugs.bareos.org/view.php?id=1450
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
[PR #1013]: https://github.com/bareos/bareos/pull/1013
[PR #1017]: https://github.com/bareos/bareos/pull/1017
[PR #1018]: https://github.com/bareos/bareos/pull/1018
[PR #1019]: https://github.com/bareos/bareos/pull/1019
[PR #1020]: https://github.com/bareos/bareos/pull/1020
[PR #1021]: https://github.com/bareos/bareos/pull/1021
[PR #1022]: https://github.com/bareos/bareos/pull/1022
[PR #1023]: https://github.com/bareos/bareos/pull/1023
[PR #1024]: https://github.com/bareos/bareos/pull/1024
[PR #1025]: https://github.com/bareos/bareos/pull/1025
[PR #1026]: https://github.com/bareos/bareos/pull/1026
[PR #1027]: https://github.com/bareos/bareos/pull/1027
[PR #1028]: https://github.com/bareos/bareos/pull/1028
[PR #1029]: https://github.com/bareos/bareos/pull/1029
[PR #1031]: https://github.com/bareos/bareos/pull/1031
[PR #1033]: https://github.com/bareos/bareos/pull/1033
[PR #1040]: https://github.com/bareos/bareos/pull/1040
[PR #1044]: https://github.com/bareos/bareos/pull/1044
[PR #1045]: https://github.com/bareos/bareos/pull/1045
[PR #1049]: https://github.com/bareos/bareos/pull/1049
[PR #1050]: https://github.com/bareos/bareos/pull/1050
[PR #1051]: https://github.com/bareos/bareos/pull/1051
[PR #1052]: https://github.com/bareos/bareos/pull/1052
[PR #1053]: https://github.com/bareos/bareos/pull/1053
[PR #1055]: https://github.com/bareos/bareos/pull/1055
[PR #1056]: https://github.com/bareos/bareos/pull/1056
[PR #1057]: https://github.com/bareos/bareos/pull/1057
[PR #1059]: https://github.com/bareos/bareos/pull/1059
[PR #1062]: https://github.com/bareos/bareos/pull/1062
[PR #1063]: https://github.com/bareos/bareos/pull/1063
[PR #1065]: https://github.com/bareos/bareos/pull/1065
[PR #1066]: https://github.com/bareos/bareos/pull/1066
[PR #1067]: https://github.com/bareos/bareos/pull/1067
[PR #1070]: https://github.com/bareos/bareos/pull/1070
[PR #1073]: https://github.com/bareos/bareos/pull/1073
[PR #1075]: https://github.com/bareos/bareos/pull/1075
[PR #1076]: https://github.com/bareos/bareos/pull/1076
[PR #1079]: https://github.com/bareos/bareos/pull/1079
[PR #1080]: https://github.com/bareos/bareos/pull/1080
[PR #1084]: https://github.com/bareos/bareos/pull/1084
[PR #1088]: https://github.com/bareos/bareos/pull/1088
[PR #1089]: https://github.com/bareos/bareos/pull/1089
[PR #1090]: https://github.com/bareos/bareos/pull/1090
[PR #1094]: https://github.com/bareos/bareos/pull/1094
[PR #1095]: https://github.com/bareos/bareos/pull/1095
[PR #1096]: https://github.com/bareos/bareos/pull/1096
[PR #1100]: https://github.com/bareos/bareos/pull/1100
[PR #1102]: https://github.com/bareos/bareos/pull/1102
[PR #1107]: https://github.com/bareos/bareos/pull/1107
[PR #1108]: https://github.com/bareos/bareos/pull/1108
[PR #1111]: https://github.com/bareos/bareos/pull/1111
[PR #1113]: https://github.com/bareos/bareos/pull/1113
[PR #1116]: https://github.com/bareos/bareos/pull/1116
[PR #1117]: https://github.com/bareos/bareos/pull/1117
[PR #1118]: https://github.com/bareos/bareos/pull/1118
[PR #1125]: https://github.com/bareos/bareos/pull/1125
[PR #1128]: https://github.com/bareos/bareos/pull/1128
[PR #1132]: https://github.com/bareos/bareos/pull/1132
[PR #1133]: https://github.com/bareos/bareos/pull/1133
[PR #1136]: https://github.com/bareos/bareos/pull/1136
[PR #1141]: https://github.com/bareos/bareos/pull/1141
[PR #1144]: https://github.com/bareos/bareos/pull/1144
[PR #1146]: https://github.com/bareos/bareos/pull/1146
[PR #1148]: https://github.com/bareos/bareos/pull/1148
[PR #1150]: https://github.com/bareos/bareos/pull/1150
[PR #1156]: https://github.com/bareos/bareos/pull/1156
[PR #1160]: https://github.com/bareos/bareos/pull/1160
[PR #1164]: https://github.com/bareos/bareos/pull/1164
[PR #1167]: https://github.com/bareos/bareos/pull/1167
[PR #1173]: https://github.com/bareos/bareos/pull/1173
[PR #1175]: https://github.com/bareos/bareos/pull/1175
[PR #1184]: https://github.com/bareos/bareos/pull/1184
[PR #1191]: https://github.com/bareos/bareos/pull/1191
[PR #1195]: https://github.com/bareos/bareos/pull/1195
[PR #1201]: https://github.com/bareos/bareos/pull/1201
[PR #1207]: https://github.com/bareos/bareos/pull/1207
[PR #1208]: https://github.com/bareos/bareos/pull/1208
[PR #1212]: https://github.com/bareos/bareos/pull/1212
[PR #1214]: https://github.com/bareos/bareos/pull/1214
[PR #1220]: https://github.com/bareos/bareos/pull/1220
[PR #1222]: https://github.com/bareos/bareos/pull/1222
[PR #1232]: https://github.com/bareos/bareos/pull/1232
[unreleased]: https://github.com/bareos/bareos/tree/master

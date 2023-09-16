All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- FreeBSD: build cleanup [PR #1382]
- build: switch to FreeBSD 12.4 [PR #1444]
- build: remove EOL distros from build matrix [PR #1472]
- build: switch from FreeBSD 13.1 to 13.2 [PR #1527]
- webui: upgrade bootstrap to version 3.4.1 [PR #1553]

## [20.0.8] - 2022-11-09

### Breaking Changes
- VMWare plugin now uses VDDK 8. As a result the minimum supported vSphere version is now 6.7.

### Documentation
- Adapt VMware plugin documentation for update to VDDK 8 [PR #1292]

### Fixed
- winbareos.nsi: fix working directory in configure.sed [PR #1290]
- webui: fix pool link in job details formatter [PR #1304] [Issue #1489]

### Removed
- packaging: do not build for EOL Ubuntu 16.04 anymore [PR #1290]

## [20.0.7] - 2022-08-05

### Changed
- jstreegrid: remove handling of IE < 8 using navigator interface to avoid warnings in chrome [PR #1142]
- doc: clarifies CheckFileChanges option not intended to be used with plugin [Issue #1452] [PR #1196]
- build: switch from FreeBSD 13.0 to 13.1 [PR #1258]
### Fixed
- webui: get volume and pool params from query instead of route [PR #1145]

### Security
- webui: update moment.js to version 2.29.2 [PR #1157] [CVE-2022-24785](https://github.com/advisories/GHSA-8hfj-j24r-96c4)
  - webui is NOT affected
  - webui does NOT use the npm server
  - webui does NOT use a user-provided locale string to directly switch moment locale


## [20.0.6] - 2022-03-14

### Breaking Changes
- when using PAM Bareos will now check authorization, too. If authorization is not configured, login will fail. See [updated documentation](https://docs.bareos.org/TasksAndConcepts/PAM.html#configuration) on how to proceed [PR #1119].

### Fixed
- debian: add missing python plugin dependencies [PR #1061] (backport of [PR #1045])
- Fix context confusion in Director's Python plugins [PR #1077]
- webui: fix deprecated notice required param follows optional param [PR #1101]
- webui: fix undefined array key warning [PR #1103]
- webui: show DIR message if ACL prevents a job rerun [PR #1112]

### Added
- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1058]

### Security
- webui: update jquery from v3.2.0 to v3.6.0 [PR #1085]
- dird: check authorization on PAM login [PR #1119] [CVE-2022-24755](https://github.com/bareos/bareos/security/advisories/GHSA-4979-8ffj-4q26)
- dird: fix memory leak on failed PAM login [PR #1119] [CVE-2022-24756](https://github.com/bareos/bareos/security/advisories/GHSA-jh55-4wgw-xc9j)

### Changed
- Don't keep volume open after acquiring a read-storage failed in migrate/copy/virtual full [PR #1114]
- webui: new login screen background and adapted logo to support Ukraine  [PR #1123]


## [20.0.5] - 2021-12-20

### Fixed
- [Issue #1374] Include zero-file incremental backups in always-incremental consolidation [PR #1000]
- fix crash in "status scheduler" command when job->client is unset [PR #1001]

## [20.0.4] - 2021-11-22

### Fixed
- Adapt Python DIR and SD plugin Baseclasses to the modernized Python plugin API [PR #957] (backport of [PR #923])
- Log LDAP info error (e.g. expired SSL cert error) [PR #970] (backport of [PR #956])
- Fix occasional "NULL volume name" error when non-busy, but blocked drive is unloaded [PR #975]
- Fix PostgreSQL create database script [PR #983]
- Unify level use with set client_min_message instruction in SQL update scripts [PR #983]

### Added
- packages: Build also for SLE_15_SP3 and openSUSE_15.3 [PR #945]
- packages: Build also for Fedora_35 [PR #976]
- cmake: check for chflags() function and enable FreeBSD File Flags support [PR #974]

### Changed
- Adapt percona-xtrabackup test to work on updated test environment [PR #978]
- Fixed libdroplet xattr.h include issue by using sys/xattr.h [PR #991]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1064] (backport of [PR #1021]) 

## [20.0.3] - 2021-09-14

### Fixed
- [Issue #579]: Unable to connect to the director from webui via ipv6 [PR #871] (backport of [PR #868])
- [Issue #1324]: Infinite loop when trying to log in with invalid account [PR #864] (backport of [PR #840])
- [Issue #1300]: some job status are not categorized properly [PR #878] (backport of [PR #874])
- [Issue #871]: UI will not load complete [PR #884] (backport of [PR #880])
- [Issue #971]: Error building tree for filenames with backslashes [PR #894] (backport of [PR #892])
- [Issue #1020]: Can not restore a client with spaces in its name [PR #894] (backport of [PR #893])
- [Issue #1235]: Special characters not escaped in translations [PR #898] (backport of [PR #887] [PR #888] [PR #889] [PR #890])
- [Issue #1251]: Error when displaying pool detail [PR #904] (backport of [PR #903])
- [Issue #1369]: webui tries to load a nonexistent file [PR #901] (backport of [PR #900])
- fix lost byte in ChunkedDevice (backport of [PR #910])
- fix director crash on "update slots" when there is a parsing issue with the autochanger or tape devices [PR #919]
- [Issue #1232]: bareos logrotate errors, reintroduce su directive in logrotate (backport of [PR #918])

### Added
- packages: Build also for Debian_11 [PR #915]

### Changed
- add job name in End Job Session output in bls tool [PR #916]
- add chromedriver options to improve reliability of selenium tests [PR #920]
- docs: Describe how to get debugging info when using the VMware plugin [PR #922]

## [20.0.2] - 2021-06-10

### Fixed
- docs: Adapted the documentation of the VMware plugin due to update to VDDK 7 [PR #845]
- fix a bug in VMware plugin where VMDK Files were created with wrong size when using the option localvmdk=yes [PR #830]
- fix possible read/write problems when using droplet with https [PR #773]
- fix memory leak in python module constants [PR #781]
- fix german localization errors [PR #786]
- fix gfapi-fd: avoid possible crash on second glfs_close() call [PR #797]
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #818] (backport of [PR #809])
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #818] (backport of [PR #809])
- [Issue #1205]: PHP 7.3 issue with compact() in HeadLink.php [PR #833] (backport of [PR #829])
- reorder acquire on migrate/copy to avoid possible deadlock [PR #828]

### Added
- added choice for the drive number in the truncate command [PR #837]
- systemtests for S3 functionalities (droplet, libcloud) now use https [PR #765]
- add "copy button" to code snippets in documentation for easy copying [PR #802]

### Changed
- repaired or added all header guards in libdroplet [PR #773]
- [Issue #1329]: If CommandACL limits any command, no messages can be read but "you have messages" is displayed. [PR #795](backport of [PR #763])

### Removed
- packaging: Do not build for EOL FreeBSD 12.1 anymore.

### Documentation
- Restore error "could not hard link" documented: what is the cause and how it can be avoided or solved. (Backport [PR #759])


## [20.0.1] - 2021-03-02

### Fixed
- fix parallel python plugin jobs [PR #732]
- fix oVirt plugin problem with config file [PR #732]
- fix crash when loading both python-fd and python3-fd plugins [PR #733]
- [Issue #1316]: storage daemon loses a configured device instance [PR #734]
- fix python-bareos for Python < 2.7.13 [PR #753]


### Added
- py2lug-fd-ovirt systemtest: use ovirt-plugin.ini config file [PR #732]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #745].
- informative debugmessage when a dynamic backend cannot be loaded [PR #746]

### Changed
- When using Python > 3.7 the postgres and libcloud plugins will cancel the job and write an error message [PR #775]
- Disabled test "statefile" for big endian, use temporary state files for all other architectures [PR #758]
- Fixed broken link in https://docs.bareos.org/IntroductionAndTutorial/WhatIsBareos.html documentation page
- Ctest now runs in scripted mode. [PR #751]


## [20.0.0] - 2020-12-16

### Fixed

- Fix memory corruption in storage daemon [PR #616]
- [Issue #1039]: Can not login in webui [PR #591]
- Improve the behaviour of some console commands [PR #604]
- Several improvements to the LDAP Plugin for filedaemon [PR #595]
- Avoid duplicate actions after submission of an action followed by a page refresh in the webui [PR #594]
- Fix several droplet timeout problems to enable connection retries [PR #561]
- Close connection pool sockets properly (client initiated connections) [PR #562]
- Check user ACL for status configuration [PR #570]
- Allow backup/restore of cephfs mounts [PR #554]
- Fix percona xtrabackup get last_lsn value [PR #551]
- Prevent director daemon crash by adding a nullptr-guard to CreateAttributesRecord() [PR #566]
- Enable testing of deprecated databases (MySQL and SQLite) [PR #573]
- Fix heartbeat thread shutdown race condition [PR #565]
- Fix build problems for several distributions [PR #569]
- Prevent from unique key violations on .bvfs_update [PR #545]
- Fix "make install" and add "make uninstall" when building from source [PR #556]
- [Issue #1257]: Prune volume command may crash the director [PR #546]
- Remove compiler warnings [PR #543]
- python-plugins: improve statp handling in localfileset plugin [PR #530]
- Fix some issues in the tray-monitor configuration [PR #539]
- Avoid a memory double-free in a spool function of the storage daemon [PR #535]
- [Issue #1246]: oVirt plugin fails to restore to local disk with KeyError [PR #534]
- Huge source code cleanup to reduce compiler warnings and increase readability [PR #479]
- Fix bscan handling of restore object data [PR #516]
- Adapt Python Bareos version number according to PEP-440 [PR #521]
- Fixed JobDetails joblog messages order [PR #509]
- Huge source code cleanup of storagedaemon Device backend [PR #498]
- [Issue #1227]: Fix fadvise bug [PR #496]
- Improve webui frontend login dialog [PR #505]
- Fixed the use of a specified since time for the run command [PR #503]
- Fixed that the status scheduler command shows jobs for disabled clients [PR #499]
- Allow Python FD Plugins to set since time [PR #504]
- Fixed a bug when backup a VM by using VM id [PR #415]
- Fixed that droplet is waiting forever if flushing a file fails: [PR #468]
- [Issue #1220]: default drive index not set to 0: [PR #472]
- Fixed a segmentation fault caused by deprecated OpenSSL functions [PR #453]
- [Issue #1211]: Bareos director and bareos fd crash when regexwhere is specified [PR #451]
- Always Incremental: Address early prune problem when vol retentions are too small [PR #423]
- [Issue #1206]: Form field restore job is not populated after client selection [PR #438]
- [Issue #1192]: Authorization key rejected by Storage daemon since upgrading director and storage daemons [PR #431]
- Allow path lengths of a messages resource to be longer that 128 bytes [PR #433]
- [Issue #1190]: Schedules without a client will not be run
- Fixed a bug where hourly schedules fail on the general rule without a day [PR #422]
- [Issue #1175]: bareos crashes with invalid character in ACL [PR #410]
- [Issue #1184]: Using syslog or an invalid message type crashes a daemon [PR #414]
- [Issue #1176]: Upgrade 18.2.5 to 19.2.5 failed while upgrading MySQL database [PR #409]
- [Issue #1174]: Reversed sort order on the restore client selection [PR #411]
- Fixed errors in client only build [PR #413]
- Fixed checksum algorithm regarding byte order [PR #412]
- Avoid a race condition in job name creation [PR #385]
- [Issue #1281]: Director crashes on memory access violation in CloseMemoryPool() and is_regex()

### Added

- Add console command setdevice [PR #686]
- Add support for Fedora 33 [PR #643]
- Add development tools for changelog-handling [PR #617]
- Added documentation and background information for the new python 3 plugin support [PR #599]
- Added the libcloud filedaemon plugin to backup S3 buckets
- Allow terabytes, tebibytes, petabytes, pebibytes, exabytes and exbibytes unit symbols when specifying sizes [PR #600]
- Add technology preview for a REST API to configure and manage Bareos [PR #587]
- Add some configuration warnings to the storagedaemon [PR #583]
- Add python3 support for filedaemon, storagedaemon and director plugins [PR #572]
- Use configurable OpenSSL "Protocol" parameter [PR #575]
- Extend bconsole show command to support JSON output format (API 2) [PR #574]
- Allow multiline quoted strings in the configuration [PR #578]
- Print toolchain details during cmake run [PR #567]
- Build libdroplet as submodule and use systemtest test [PR #560]
- Run systemtests during build, build packages from source root directory [PR #559]
- Add pkglists to documentation for all OS we build for [PR #557]
- Merge VMware plugin code into core subdirectories, add system test [PR #544]
- The VMware plugin now allows to use an optional config file to specify plugin options [PR #544]
- Removed the dependency to pyvmomi from rpm and deb packages [PR #667]
- Add next pool selection to custom job run form in the webui [PR #518]
- In job reports display who triggered a job (scheduler, user, ..) [PR #532]
- Added VirtualFull option for manual defined Jobs [PR #508]
- Add support for Arch and Manjaro Linux [PR #513]
- Document Verify VolumeToCatalog limitation [PR #507]
- Document drawbacks for job intermixing [PR #506]
- Add postgres-backup filedaemon plugin with incremental backups and point in time restore [PR #502]
- Write job- and file-choice to audit log on restore [PR #493]
- Add infrastructure for configuration warnings [PR #492]
- Add logging parameters to gfapi backend [PR #455]
- Add a security policy (`SECURITY.md`) to the project [PR #463]
- Pre-fill restore form element "replace" according to selected restore job in the webui: [PR #456]
- Add documentation for a critical bscan limitation: [PR #460]
- Pre-fill restore location form element according to selected restore job in webui [PR #454]
- Add more logging messages to NDMP interface [PR #448]
- Add start and finish time in the webui-joblist [PR #429]
- Add a database upgrade test [PR #406]
- Add recommendation for PHP 7 to documentation [PR #420]
- Add encryption signature test [PR #404]
- Add percona extra backup [PR #386]
- Add test for client initiated backup [PR #383]

### Changed

- Release-notes / changelog have been moved to CHANGELOG.md [PR #613]
- Bareos now requires at least CMake 3.12 to build
- Updated many localization files [PR #609]
- Display archived jobs on list backups command [PR #581]
- Use a configurable VDI timeout when requesting the MSSQL config [PR #571]
- Improved the formatting of job log entries in the webui [PR #525]
- Cleanup unused images [PR #538]
- Use precise file names for public and private key files to make documentation clear [PR #537]
- Significantly speed-up bareos-dbcopy [PR #524]
- Publish PythonBareos to PyPI.org using Github Actions [PR #514]
- Updated documentation for tests [PR #497]
- Display numeric values of UID and GID in addition to user and group name in webui [PR #488]
- Display file permissions in restore dialog of webui [PR #477]
- Improve warnings on Maximum Block Size of the droplet backend: [PR #476]
- Improve the selection of a specific file version restore in the webui: [PR #471]
- Only allow configuration maximum concurrent jobs of 0 or 1 when using droplet: [PR #465]
- Improve filtering on webui-jobs-bootstrap-table [PR #444]
- Allow systemtests to run on installed packaged [PR #400]
- Allow systemtests to be run in parallel [PR #392]
- Disable systemtests on non compatible systems [PR #387]
- Update ovirt plugin documentation [PR #384]
- Improve ovirt plugin

### Deprecated
- Using MySQL as a catalog backend is now deprecated [PR #564]
- Using Rados as a storage backend is now deprecated [PR #672]
- With full Python 3 support, using Python 2 is now considered deprecated
- Plugin bareos-fd-ldap is deprecated, because it requires Python 2


### Removed
- Remove support for creating MySQL / SQLite catalogs [PR #563] [PR #603]

### Security

- Do not accept cram challenge if own hostname is used [PR #536] [CVE-2020-4042](https://github.com/bareos/bareos/security/advisories/GHSA-vqpj-2vhj-h752)
- [Issue #1210]: Heap overflow in director when running a verify job against a malicious filedaemon [CVE-2020-11061](https://github.com/bareos/bareos/security/advisories/GHSA-mm45-cg35-54j4)


[20.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.0
[20.0.1]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.1
[20.0.2]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.2
[20.0.3]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.3
[20.0.4]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.4
[20.0.5]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.5
[20.0.6]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.6
[20.0.7]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.7
[20.0.8]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.8
[Issue #579]: https://bugs.bareos.org/view.php?id=579
[Issue #871]: https://bugs.bareos.org/view.php?id=871
[Issue #971]: https://bugs.bareos.org/view.php?id=971
[Issue #1020]: https://bugs.bareos.org/view.php?id=1020
[Issue #1039]: https://bugs.bareos.org/view.php?id=1039
[Issue #1174]: https://bugs.bareos.org/view.php?id=1174
[Issue #1175]: https://bugs.bareos.org/view.php?id=1175
[Issue #1176]: https://bugs.bareos.org/view.php?id=1176
[Issue #1184]: https://bugs.bareos.org/view.php?id=1184
[Issue #1190]: https://bugs.bareos.org/view.php?id=1190
[Issue #1192]: https://bugs.bareos.org/view.php?id=1192
[Issue #1205]: https://bugs.bareos.org/view.php?id=1205
[Issue #1206]: https://bugs.bareos.org/view.php?id=1206
[Issue #1210]: https://bugs.bareos.org/view.php?id=1210
[Issue #1211]: https://bugs.bareos.org/view.php?id=1211
[Issue #1220]: https://bugs.bareos.org/view.php?id=1220
[Issue #1227]: https://bugs.bareos.org/view.php?id=1227
[Issue #1232]: https://bugs.bareos.org/view.php?id=1232
[Issue #1235]: https://bugs.bareos.org/view.php?id=1235
[Issue #1246]: https://bugs.bareos.org/view.php?id=1246
[Issue #1251]: https://bugs.bareos.org/view.php?id=1251
[Issue #1257]: https://bugs.bareos.org/view.php?id=1257
[Issue #1281]: https://bugs.bareos.org/view.php?id=1281
[Issue #1300]: https://bugs.bareos.org/view.php?id=1300
[Issue #1316]: https://bugs.bareos.org/view.php?id=1316
[Issue #1324]: https://bugs.bareos.org/view.php?id=1324
[Issue #1329]: https://bugs.bareos.org/view.php?id=1329
[Issue #1369]: https://bugs.bareos.org/view.php?id=1369
[Issue #1374]: https://bugs.bareos.org/view.php?id=1374
[Issue #1452]: https://bugs.bareos.org/view.php?id=1452
[Issue #1489]: https://bugs.bareos.org/view.php?id=1489
[PR #383]: https://github.com/bareos/bareos/pull/383
[PR #384]: https://github.com/bareos/bareos/pull/384
[PR #385]: https://github.com/bareos/bareos/pull/385
[PR #386]: https://github.com/bareos/bareos/pull/386
[PR #387]: https://github.com/bareos/bareos/pull/387
[PR #392]: https://github.com/bareos/bareos/pull/392
[PR #400]: https://github.com/bareos/bareos/pull/400
[PR #404]: https://github.com/bareos/bareos/pull/404
[PR #406]: https://github.com/bareos/bareos/pull/406
[PR #409]: https://github.com/bareos/bareos/pull/409
[PR #410]: https://github.com/bareos/bareos/pull/410
[PR #411]: https://github.com/bareos/bareos/pull/411
[PR #412]: https://github.com/bareos/bareos/pull/412
[PR #413]: https://github.com/bareos/bareos/pull/413
[PR #414]: https://github.com/bareos/bareos/pull/414
[PR #415]: https://github.com/bareos/bareos/pull/415
[PR #420]: https://github.com/bareos/bareos/pull/420
[PR #422]: https://github.com/bareos/bareos/pull/422
[PR #423]: https://github.com/bareos/bareos/pull/423
[PR #429]: https://github.com/bareos/bareos/pull/429
[PR #431]: https://github.com/bareos/bareos/pull/431
[PR #433]: https://github.com/bareos/bareos/pull/433
[PR #438]: https://github.com/bareos/bareos/pull/438
[PR #444]: https://github.com/bareos/bareos/pull/444
[PR #448]: https://github.com/bareos/bareos/pull/448
[PR #451]: https://github.com/bareos/bareos/pull/451
[PR #453]: https://github.com/bareos/bareos/pull/453
[PR #454]: https://github.com/bareos/bareos/pull/454
[PR #455]: https://github.com/bareos/bareos/pull/455
[PR #456]: https://github.com/bareos/bareos/pull/456
[PR #460]: https://github.com/bareos/bareos/pull/460
[PR #463]: https://github.com/bareos/bareos/pull/463
[PR #465]: https://github.com/bareos/bareos/pull/465
[PR #468]: https://github.com/bareos/bareos/pull/468
[PR #471]: https://github.com/bareos/bareos/pull/471
[PR #472]: https://github.com/bareos/bareos/pull/472
[PR #476]: https://github.com/bareos/bareos/pull/476
[PR #477]: https://github.com/bareos/bareos/pull/477
[PR #479]: https://github.com/bareos/bareos/pull/479
[PR #488]: https://github.com/bareos/bareos/pull/488
[PR #492]: https://github.com/bareos/bareos/pull/492
[PR #493]: https://github.com/bareos/bareos/pull/493
[PR #496]: https://github.com/bareos/bareos/pull/496
[PR #497]: https://github.com/bareos/bareos/pull/497
[PR #498]: https://github.com/bareos/bareos/pull/498
[PR #499]: https://github.com/bareos/bareos/pull/499
[PR #502]: https://github.com/bareos/bareos/pull/502
[PR #503]: https://github.com/bareos/bareos/pull/503
[PR #504]: https://github.com/bareos/bareos/pull/504
[PR #505]: https://github.com/bareos/bareos/pull/505
[PR #506]: https://github.com/bareos/bareos/pull/506
[PR #507]: https://github.com/bareos/bareos/pull/507
[PR #508]: https://github.com/bareos/bareos/pull/508
[PR #509]: https://github.com/bareos/bareos/pull/509
[PR #513]: https://github.com/bareos/bareos/pull/513
[PR #514]: https://github.com/bareos/bareos/pull/514
[PR #516]: https://github.com/bareos/bareos/pull/516
[PR #518]: https://github.com/bareos/bareos/pull/518
[PR #521]: https://github.com/bareos/bareos/pull/521
[PR #524]: https://github.com/bareos/bareos/pull/524
[PR #525]: https://github.com/bareos/bareos/pull/525
[PR #530]: https://github.com/bareos/bareos/pull/530
[PR #532]: https://github.com/bareos/bareos/pull/532
[PR #534]: https://github.com/bareos/bareos/pull/534
[PR #535]: https://github.com/bareos/bareos/pull/535
[PR #536]: https://github.com/bareos/bareos/pull/536
[PR #537]: https://github.com/bareos/bareos/pull/537
[PR #538]: https://github.com/bareos/bareos/pull/538
[PR #539]: https://github.com/bareos/bareos/pull/539
[PR #543]: https://github.com/bareos/bareos/pull/543
[PR #544]: https://github.com/bareos/bareos/pull/544
[PR #545]: https://github.com/bareos/bareos/pull/545
[PR #546]: https://github.com/bareos/bareos/pull/546
[PR #551]: https://github.com/bareos/bareos/pull/551
[PR #554]: https://github.com/bareos/bareos/pull/554
[PR #556]: https://github.com/bareos/bareos/pull/556
[PR #557]: https://github.com/bareos/bareos/pull/557
[PR #559]: https://github.com/bareos/bareos/pull/559
[PR #560]: https://github.com/bareos/bareos/pull/560
[PR #561]: https://github.com/bareos/bareos/pull/561
[PR #562]: https://github.com/bareos/bareos/pull/562
[PR #563]: https://github.com/bareos/bareos/pull/563
[PR #564]: https://github.com/bareos/bareos/pull/564
[PR #565]: https://github.com/bareos/bareos/pull/565
[PR #566]: https://github.com/bareos/bareos/pull/566
[PR #567]: https://github.com/bareos/bareos/pull/567
[PR #569]: https://github.com/bareos/bareos/pull/569
[PR #570]: https://github.com/bareos/bareos/pull/570
[PR #571]: https://github.com/bareos/bareos/pull/571
[PR #572]: https://github.com/bareos/bareos/pull/572
[PR #573]: https://github.com/bareos/bareos/pull/573
[PR #574]: https://github.com/bareos/bareos/pull/574
[PR #575]: https://github.com/bareos/bareos/pull/575
[PR #578]: https://github.com/bareos/bareos/pull/578
[PR #581]: https://github.com/bareos/bareos/pull/581
[PR #583]: https://github.com/bareos/bareos/pull/583
[PR #587]: https://github.com/bareos/bareos/pull/587
[PR #591]: https://github.com/bareos/bareos/pull/591
[PR #594]: https://github.com/bareos/bareos/pull/594
[PR #595]: https://github.com/bareos/bareos/pull/595
[PR #599]: https://github.com/bareos/bareos/pull/599
[PR #600]: https://github.com/bareos/bareos/pull/600
[PR #603]: https://github.com/bareos/bareos/pull/603
[PR #604]: https://github.com/bareos/bareos/pull/604
[PR #609]: https://github.com/bareos/bareos/pull/609
[PR #613]: https://github.com/bareos/bareos/pull/613
[PR #616]: https://github.com/bareos/bareos/pull/616
[PR #617]: https://github.com/bareos/bareos/pull/617
[PR #643]: https://github.com/bareos/bareos/pull/643
[PR #667]: https://github.com/bareos/bareos/pull/667
[PR #672]: https://github.com/bareos/bareos/pull/672
[PR #686]: https://github.com/bareos/bareos/pull/686
[PR #732]: https://github.com/bareos/bareos/pull/732
[PR #733]: https://github.com/bareos/bareos/pull/733
[PR #734]: https://github.com/bareos/bareos/pull/734
[PR #745]: https://github.com/bareos/bareos/pull/745
[PR #746]: https://github.com/bareos/bareos/pull/746
[PR #751]: https://github.com/bareos/bareos/pull/751
[PR #753]: https://github.com/bareos/bareos/pull/753
[PR #758]: https://github.com/bareos/bareos/pull/758
[PR #759]: https://github.com/bareos/bareos/pull/759
[PR #763]: https://github.com/bareos/bareos/pull/763
[PR #765]: https://github.com/bareos/bareos/pull/765
[PR #773]: https://github.com/bareos/bareos/pull/773
[PR #775]: https://github.com/bareos/bareos/pull/775
[PR #781]: https://github.com/bareos/bareos/pull/781
[PR #786]: https://github.com/bareos/bareos/pull/786
[PR #795]: https://github.com/bareos/bareos/pull/795
[PR #797]: https://github.com/bareos/bareos/pull/797
[PR #802]: https://github.com/bareos/bareos/pull/802
[PR #809]: https://github.com/bareos/bareos/pull/809
[PR #818]: https://github.com/bareos/bareos/pull/818
[PR #828]: https://github.com/bareos/bareos/pull/828
[PR #829]: https://github.com/bareos/bareos/pull/829
[PR #830]: https://github.com/bareos/bareos/pull/830
[PR #833]: https://github.com/bareos/bareos/pull/833
[PR #837]: https://github.com/bareos/bareos/pull/837
[PR #840]: https://github.com/bareos/bareos/pull/840
[PR #845]: https://github.com/bareos/bareos/pull/845
[PR #864]: https://github.com/bareos/bareos/pull/864
[PR #868]: https://github.com/bareos/bareos/pull/868
[PR #871]: https://github.com/bareos/bareos/pull/871
[PR #874]: https://github.com/bareos/bareos/pull/874
[PR #878]: https://github.com/bareos/bareos/pull/878
[PR #880]: https://github.com/bareos/bareos/pull/880
[PR #884]: https://github.com/bareos/bareos/pull/884
[PR #887]: https://github.com/bareos/bareos/pull/887
[PR #888]: https://github.com/bareos/bareos/pull/888
[PR #889]: https://github.com/bareos/bareos/pull/889
[PR #890]: https://github.com/bareos/bareos/pull/890
[PR #892]: https://github.com/bareos/bareos/pull/892
[PR #893]: https://github.com/bareos/bareos/pull/893
[PR #894]: https://github.com/bareos/bareos/pull/894
[PR #898]: https://github.com/bareos/bareos/pull/898
[PR #900]: https://github.com/bareos/bareos/pull/900
[PR #901]: https://github.com/bareos/bareos/pull/901
[PR #903]: https://github.com/bareos/bareos/pull/903
[PR #904]: https://github.com/bareos/bareos/pull/904
[PR #910]: https://github.com/bareos/bareos/pull/910
[PR #915]: https://github.com/bareos/bareos/pull/915
[PR #916]: https://github.com/bareos/bareos/pull/916
[PR #918]: https://github.com/bareos/bareos/pull/918
[PR #919]: https://github.com/bareos/bareos/pull/919
[PR #920]: https://github.com/bareos/bareos/pull/920
[PR #922]: https://github.com/bareos/bareos/pull/922
[PR #923]: https://github.com/bareos/bareos/pull/923
[PR #945]: https://github.com/bareos/bareos/pull/945
[PR #956]: https://github.com/bareos/bareos/pull/956
[PR #957]: https://github.com/bareos/bareos/pull/957
[PR #970]: https://github.com/bareos/bareos/pull/970
[PR #974]: https://github.com/bareos/bareos/pull/974
[PR #975]: https://github.com/bareos/bareos/pull/975
[PR #976]: https://github.com/bareos/bareos/pull/976
[PR #978]: https://github.com/bareos/bareos/pull/978
[PR #983]: https://github.com/bareos/bareos/pull/983
[PR #991]: https://github.com/bareos/bareos/pull/991
[PR #1000]: https://github.com/bareos/bareos/pull/1000
[PR #1001]: https://github.com/bareos/bareos/pull/1001
[PR #1021]: https://github.com/bareos/bareos/pull/1021
[PR #1045]: https://github.com/bareos/bareos/pull/1045
[PR #1058]: https://github.com/bareos/bareos/pull/1058
[PR #1061]: https://github.com/bareos/bareos/pull/1061
[PR #1064]: https://github.com/bareos/bareos/pull/1064
[PR #1077]: https://github.com/bareos/bareos/pull/1077
[PR #1085]: https://github.com/bareos/bareos/pull/1085
[PR #1101]: https://github.com/bareos/bareos/pull/1101
[PR #1103]: https://github.com/bareos/bareos/pull/1103
[PR #1112]: https://github.com/bareos/bareos/pull/1112
[PR #1114]: https://github.com/bareos/bareos/pull/1114
[PR #1119]: https://github.com/bareos/bareos/pull/1119
[PR #1123]: https://github.com/bareos/bareos/pull/1123
[PR #1142]: https://github.com/bareos/bareos/pull/1142
[PR #1145]: https://github.com/bareos/bareos/pull/1145
[PR #1157]: https://github.com/bareos/bareos/pull/1157
[PR #1196]: https://github.com/bareos/bareos/pull/1196
[PR #1258]: https://github.com/bareos/bareos/pull/1258
[PR #1290]: https://github.com/bareos/bareos/pull/1290
[PR #1292]: https://github.com/bareos/bareos/pull/1292
[PR #1304]: https://github.com/bareos/bareos/pull/1304
[PR #1382]: https://github.com/bareos/bareos/pull/1382
[PR #1444]: https://github.com/bareos/bareos/pull/1444
[PR #1472]: https://github.com/bareos/bareos/pull/1472
[PR #1527]: https://github.com/bareos/bareos/pull/1527
[PR #1553]: https://github.com/bareos/bareos/pull/1553
[unreleased]: https://github.com/bareos/bareos/tree/master

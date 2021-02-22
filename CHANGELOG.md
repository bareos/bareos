All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [20.0.1] - 2020-xx-xx

### Fixed
- fix parallel python plugin jobs [PR #732]
- fix oVirt plugin problem with config file [PR #732]
- fix crash when loading both python-fd and python3-fd plugins [PR #733]

### Added
- py2lug-fd-ovirt systemtest: use ovirt-plugin.ini config file [PR #732]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #723].

### Changed

### Deprecated

### Removed

### Security


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
[Issue #1039]: https://bugs.bareos.org/view.php?id=1039
[Issue #1174]: https://bugs.bareos.org/view.php?id=1174
[Issue #1175]: https://bugs.bareos.org/view.php?id=1175
[Issue #1176]: https://bugs.bareos.org/view.php?id=1176
[Issue #1184]: https://bugs.bareos.org/view.php?id=1184
[Issue #1190]: https://bugs.bareos.org/view.php?id=1190
[Issue #1192]: https://bugs.bareos.org/view.php?id=1192
[Issue #1206]: https://bugs.bareos.org/view.php?id=1206
[Issue #1210]: https://bugs.bareos.org/view.php?id=1210
[Issue #1211]: https://bugs.bareos.org/view.php?id=1211
[Issue #1220]: https://bugs.bareos.org/view.php?id=1220
[Issue #1227]: https://bugs.bareos.org/view.php?id=1227
[Issue #1246]: https://bugs.bareos.org/view.php?id=1246
[Issue #1257]: https://bugs.bareos.org/view.php?id=1257
[Issue #1281]: https://bugs.bareos.org/view.php?id=1281
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
[PR #730]: https://github.com/bareos/bareos/pull/730
[PR #733]: https://github.com/bareos/bareos/pull/733
[unreleased]: https://github.com/bareos/bareos/tree/master

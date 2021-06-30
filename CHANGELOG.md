All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- docs: Adapted the documentation of the VMware plugin due to update to VDDK 7 [PR #844]
- fix a bug in VMware plugin where VMDK Files were created with wrong size when using the option localvmdk=yes [PR #826]
- fix a bug where the restore browser would not recognize globbing wildcards in paths [PR #801]
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #809]
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #809]
- [bug-0001194]: when doing an accurate incremental backup, if there is a database error, a full backup is done instead of reporting the error [PR #810]
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

### Added
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
- packages: Build also for openSUSE Leap 15.3 [PR #870]

### Changed
- core: systemd service: change daemon type from forking to simple and start daemons in foreground [PR #824]
- systemtests: define variable BackupDirectory globally [PR #780]
- systemtests: run all systemstests with ``set -o pipefail`` [PR #780]
- core: cleanup systemd service dependencies: Requires network.target, but start after the network-online.target [PR #700]
- core: Make the jansson library mandatory when compiling the Bareos Director [PR #793]
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
- packages: Build also for Fedora_34 [PR #869]
- postgresql filedaemon plugin: switched from psycopg2 to pg8000, dropped support for python2.

### Deprecated

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

### Security

### Documentation
- Restore error "could not hard link" documented: what is the cause and how it can be avoided or solved. [PR #759]
- Developer guide: add small chapter about c++ exceptions. [PR #777]

[Issue #1205]: https://bugs.bareos.org/view.php?id=1205
[Issue #1316]: https://bugs.bareos.org/view.php?id=1316
[Issue #1329]: https://bugs.bareos.org/view.php?id=1329
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
[PR #844]: https://github.com/bareos/bareos/pull/844
[PR #850]: https://github.com/bareos/bareos/pull/850
[PR #858]: https://github.com/bareos/bareos/pull/858
[unreleased]: https://github.com/bareos/bareos/tree/master

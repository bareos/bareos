All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #809]
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #809]
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

### Added
- systemtests for S3 functionalities (droplet, libcloud) now use https [PR #765]
- added reload commands to systemd service [PR #694]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #723].
- added an informative debugmessage when a dynamic backend cannot be loaded [PR #740]
- support for shorter date formats, where shorter dates are compensated with lowest value possible to make a full date [PR #707]
- added external repo bareos-contrib as subtree [PR #752]
- add "copy button" to code snippets in documentation for easy copying [PR #802]

### Changed
. core: Make the jansson library mandatory when compiling the Bareos Director [PR #793]
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

### Deprecated

### Removed
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

[unreleased]: https://github.com/bareos/bareos/tree/master

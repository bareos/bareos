All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Fixed
- cats: fix version.map.in [PR #2064]
- webui: fix for PHP < 7.3 [PR #2067]
- cmake: fix MARIADB_MYSQL_INSTALL_DB_SCRIPT usage [PR #2040]
- Fix building ndmjob program [PR #2079]
- bareos-config-libs: double quote dbconfig values [PR #2111]
- freebsd: automate creation of pkg-plist.database-postgresql [PR #2102]
- plugin: postgresql use integer for savepkt.object_index [PR #2132]
- bconsole: enable app icon on windows [PR #2105]

### Documentation
- docs: fix grpc-fd plugin call [PR #2068]

### Added
- added build-dep to libutfcpp-dev in debian-like environments [PR #2056]

### Changed
- plugins: Fix typo in postgresql plugin [PR #2066]
- Sync EvpDigest between OpenSSL <1.1 and 1.1+ [PR #2086]
- winbareos-native.nsi: do not package python3 plugins [PR #2076]
- logrotate: add mtx-changer debug log config [PR #2039]
- cmake: add cmake check whether tirpc is installed [PR #2109]
- bconsole: require only one password in the configuration [PR #2116]

[PR #2039]: https://github.com/bareos/bareos/pull/2039
[PR #2040]: https://github.com/bareos/bareos/pull/2040
[PR #2056]: https://github.com/bareos/bareos/pull/2056
[PR #2064]: https://github.com/bareos/bareos/pull/2064
[PR #2066]: https://github.com/bareos/bareos/pull/2066
[PR #2067]: https://github.com/bareos/bareos/pull/2067
[PR #2068]: https://github.com/bareos/bareos/pull/2068
[PR #2076]: https://github.com/bareos/bareos/pull/2076
[PR #2079]: https://github.com/bareos/bareos/pull/2079
[PR #2086]: https://github.com/bareos/bareos/pull/2086
[PR #2102]: https://github.com/bareos/bareos/pull/2102
[PR #2105]: https://github.com/bareos/bareos/pull/2105
[PR #2109]: https://github.com/bareos/bareos/pull/2109
[PR #2111]: https://github.com/bareos/bareos/pull/2111
[PR #2116]: https://github.com/bareos/bareos/pull/2116
[PR #2132]: https://github.com/bareos/bareos/pull/2132
[unreleased]: https://github.com/bareos/bareos/tree/master

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Fixed
- cats: fix version.map.in [PR #2064]
- webui: fix for PHP < 7.3 [PR #2067]
- cmake: fix MARIADB_MYSQL_INSTALL_DB_SCRIPT usage [PR #2040]
- Fix building ndmjob program [PR #2079]

### Documentation
- docs: fix grpc-fd plugin call [PR #2068]

### Added
- added build-dep to libutfcpp-dev in debian-like environments [PR #2056]

### Changed
- plugins: Fix typo in postgresql plugin [PR #2066]
- Sync EvpDigest between OpenSSL <1.1 and 1.1+ [PR #2086]

[PR #2040]: https://github.com/bareos/bareos/pull/2040
[PR #2056]: https://github.com/bareos/bareos/pull/2056
[PR #2064]: https://github.com/bareos/bareos/pull/2064
[PR #2066]: https://github.com/bareos/bareos/pull/2066
[PR #2067]: https://github.com/bareos/bareos/pull/2067
[PR #2068]: https://github.com/bareos/bareos/pull/2068
[PR #2079]: https://github.com/bareos/bareos/pull/2079
[PR #2086]: https://github.com/bareos/bareos/pull/2086
[unreleased]: https://github.com/bareos/bareos/tree/master

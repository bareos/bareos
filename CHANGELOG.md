All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
￼- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1043]
￼
### Breaking Changes

### Fixed
- debian: Let dbconfig create the Bareos catalog also with LC_COLLATE='C' and LC_CTYPE='C'. The create_bareos_database script did always do so. Requires dbconfig >= 2.0.21 [PR #1031]
- fix wrong `packages_dir` in restapi workflow, so restapi packages will be released to PyPI [PR #1033]
- core cats: Add IF EXISTS in drop table statements fix for bug #1409 (Allow usage of ExitOnFatal) [PR #1035]
- sql_get.cc: fix error logging in GetJobRecord() for jobname #1042
- webui: fix empty job timeline issue if date.timezone is not set in php.ini [PR #1051]
- Fix for wrong update message when updating all volumes from all pools with no existing volumes [PR #1015]

### Added
- tests: simplify test coverage analysis [PR #1010]
- docs: Add chapter for mariabackup db plugin [PR #1016]
- dird: extend the list command to be able to query volumes and pools by ID [PR #1041]
- systemtests: wait for mariadb shutdown [PR #1048]

### Changed

### Deprecated

### Removed

### Security

### Documentation

- cleanup update section [PR #1054]
- clarifies MySQL catalog migration process [PR #1054]
- split `Howtos.rst` file into one file per section [PR #1054]
- split the very long `Plugins.rst` file into one file per Bareos plugin [PR #1046]



[PR #1010]: https://github.com/bareos/bareos/pull/1010
[PR #1015]: https://github.com/bareos/bareos/pull/1015
[PR #1016]: https://github.com/bareos/bareos/pull/1016
[PR #1031]: https://github.com/bareos/bareos/pull/1031
[PR #1033]: https://github.com/bareos/bareos/pull/1033
[PR #1035]: https://github.com/bareos/bareos/pull/1035
[PR #1041]: https://github.com/bareos/bareos/pull/1041
[PR #1043]: https://github.com/bareos/bareos/pull/1043
[PR #1046]: https://github.com/bareos/bareos/pull/1046
[PR #1048]: https://github.com/bareos/bareos/pull/1048
[PR #1051]: https://github.com/bareos/bareos/pull/1051
[PR #1054]: https://github.com/bareos/bareos/pull/1054
[unreleased]: https://github.com/bareos/bareos/tree/master

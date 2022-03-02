All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
￼- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1043]
- packaging: installation and upgrade will check for the presence of :file:`.enable-cap_sys_rawio` in your bareos config dir and will configure the required cap_sys_rawio capabilities [PR #1057]

### Breaking Changes

### Fixed
- NDMP_BAREOS: support autoxflate plugin [PR #1013]
- debian: Let dbconfig create the Bareos catalog also with LC_COLLATE='C' and LC_CTYPE='C'. The create_bareos_database script did always do so. Requires dbconfig >= 2.0.21 [PR #1031]
- fix wrong `packages_dir` in restapi workflow, so restapi packages will be released to PyPI [PR #1033]
- core cats: Add IF EXISTS in drop table statements fix for bug #1409 (Allow usage of ExitOnFatal) [PR #1035]
- sql_get.cc: fix error logging in GetJobRecord() for jobname #1042
- webui: fix empty job timeline issue if date.timezone is not set in php.ini [PR #1051]
- Fix for wrong update message when updating all volumes from all pools with no existing volumes [PR #1015]
- Fix context confusion in Director's Python plugins [PR #1047]
- Fix several cases of undefined behaviour, memory corruption and memory leaks [PR #1060]

### Added
- Python plugins: add default module_path to search path [PR #1038]
- tests: simplify test coverage analysis [PR #1010]
- docs: Add chapter for mariabackup db plugin [PR #1016]
- dird: extend the list command to be able to query volumes and pools by ID [PR #1041]
- systemtests: wait for mariadb shutdown [PR #1048]

### Changed
- webui: remove an unnecessary .bvfs_get_jobids and buildSubtree() call [PR #1050]
- git: set merge strategy for CHANGELOG.md to union [PR #1062]
- webui: add timeline chart by jobs [PR #1059]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1021] 
- dir, stored: start statistics threads only if needed [PR #1040]
- gitignore: cleanup .gitignore files [PR #1067]

### Deprecated

### Removed

### Security

### Documentation

- cleanup update section [PR #1054]
- clarifies MySQL catalog migration process [PR #1054]
- split `Howtos.rst` file into one file per section [PR #1054]
- split the very long `Plugins.rst` file into one file per Bareos plugin [PR #1046]
- rework SD plugin scsicrypto linux sg_io ioctl subsection for cap_sys_rawio [PR #1057]
- improve action Python plugin documentation, by removing File in Fileset example [PR #1079]


[PR #1010]: https://github.com/bareos/bareos/pull/1010
[PR #1015]: https://github.com/bareos/bareos/pull/1015
[PR #1016]: https://github.com/bareos/bareos/pull/1016
[PR #1021]: https://github.com/bareos/bareos/pull/1021
[PR #1031]: https://github.com/bareos/bareos/pull/1031
[PR #1033]: https://github.com/bareos/bareos/pull/1033
[PR #1035]: https://github.com/bareos/bareos/pull/1035
[PR #1038]: https://github.com/bareos/bareos/pull/1038
[PR #1041]: https://github.com/bareos/bareos/pull/1041
[PR #1043]: https://github.com/bareos/bareos/pull/1043
[PR #1046]: https://github.com/bareos/bareos/pull/1046
[PR #1047]: https://github.com/bareos/bareos/pull/1047
[PR #1048]: https://github.com/bareos/bareos/pull/1048
[PR #1050]: https://github.com/bareos/bareos/pull/1050
[PR #1051]: https://github.com/bareos/bareos/pull/1051
[PR #1054]: https://github.com/bareos/bareos/pull/1054
[PR #1057]: https://github.com/bareos/bareos/pull/1057
[PR #1059]: https://github.com/bareos/bareos/pull/1059
[PR #1062]: https://github.com/bareos/bareos/pull/1062
[PR #1079]: https://github.com/bareos/bareos/pull/1079
[unreleased]: https://github.com/bareos/bareos/tree/master

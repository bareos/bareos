All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

### Removed
- remove no longer used pkglists [PR #1335]

### Changed
- cats: fix issue where `startfile` field gets wrongly updated [PR #1346]
- Python Plugins: Avoid pop(0) performance impact [PR #1351]

### Fixed
- Fix gcc warnings in ndmjob program [PR #1343]

[PR #1335]: https://github.com/bareos/bareos/pull/1335
[PR #1343]: https://github.com/bareos/bareos/pull/1343
[PR #1346]: https://github.com/bareos/bareos/pull/1346
[PR #1351]: https://github.com/bareos/bareos/pull/1351
[unreleased]: https://github.com/bareos/bareos/tree/master

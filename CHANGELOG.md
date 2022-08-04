All notable changes to this project branch will be documented in this file.

However, the release notes still reside in the sphinx documentation in
`docs/manuals/source/Appendix/ReleaseNotes.rst` and will be updated from this
file when a release is made.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## [Unreleased]

## [19.2.13] - 2022-08-04

### Changed
- jstreegrid: remove handling of IE < 8 using navigator interface to avoid warnings in chrome [PR #1143]

### Security
- webui: update moment.js to version 2.29.2 [PR #1158] [CVE-2022-24785](https://github.com/advisories/GHSA-8hfj-j24r-96c4)
  - webui is NOT affected
  - webui does NOT use the npm server
  - webui does NOT use a user-provided locale string to directly switch moment locale

## [19.2.12] - 2022-03-14

### Breaking Changes
- when using PAM Bareos will now check authorization, too. If authorization is not configured, login will fail. See [updated documentation](https://docs.bareos.org/TasksAndConcepts/PAM.html#configuration) on how to proceed [PR #1121].

### Fixed
- fix crash in "status scheduler" command when job->client is unset [PR #1002]

### Added

### Changed
- OS support: removed EOL RHEL-6 and CentOS-6 build [PR #1086]
- webui: introduce support Ukraine theme [PR #1124]

### Security
- webui: update jquery from v3.2.0 to v3.6.0 [PR #1086]
- dird: check authorization on PAM login [PR #1121] [CVE-2022-24755](https://github.com/bareos/bareos/security/advisories/GHSA-4979-8ffj-4q26)
- dird: fix memory leak on failed PAM login [PR #1121] [CVE-2022-24756](https://github.com/bareos/bareos/security/advisories/GHSA-jh55-4wgw-xc9j)

[19.2.12]: https://github.com/bareos/bareos/releases/tag/Release%2F19.2.12
[19.2.13]: https://github.com/bareos/bareos/releases/tag/Release%2F19.2.13
[PR #1002]: https://github.com/bareos/bareos/pull/1002
[PR #1086]: https://github.com/bareos/bareos/pull/1086
[PR #1121]: https://github.com/bareos/bareos/pull/1121
[PR #1124]: https://github.com/bareos/bareos/pull/1124
[PR #1143]: https://github.com/bareos/bareos/pull/1143
[PR #1158]: https://github.com/bareos/bareos/pull/1158
[unreleased]: https://github.com/bareos/bareos/tree/master

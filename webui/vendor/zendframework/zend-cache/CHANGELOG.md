# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.4.8 - 2015-09-15

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- A fix was provided for [ZF2015-07](http://framework.zend.com/security/advisory/ZF2015-07),
  ensuring that any directories or files created by the component use umask 0002
  in order to prevent arbitrary local execution and/or local privilege
  escalation.

# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.0 - 2016-02-9

### Added

- [zendframework/zend-console#16](https://github.com/zendframework/zend-console/pull/16) updates,
  reorganizes, and publishes the documentation to
  https://docs.laminas.dev/laminas-console

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-console#13](https://github.com/zendframework/zend-console/pull/13) updates the
  component to make it forwards-compatible with the laminas-stdlib and
  laminas-servicemanager v3 versions.
- [zendframework/zend-console#4](https://github.com/zendframework/zend-console/pull/4) fixes an error in
  `getTitle()` whereby the `$output` array was being incorrectly used as a
  string.
- [zendframework/zend-console#12](https://github.com/zendframework/zend-console/pull/12) updates the
  `Laminas\Console\Prompt\Char::show()` method to call on the composed adapter's
  `write()`/`writeLine()` methods instead of calling `echo()`.

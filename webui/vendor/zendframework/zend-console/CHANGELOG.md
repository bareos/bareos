# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.0 - 2016-02-9

### Added

- [#16](https://github.com/zendframework/zend-console/pull/16) updates,
  reorganizes, and publishes the documentation to
  https://zendframework.github.io/zend-console

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#13](https://github.com/zendframework/zend-console/pull/13) updates the
  component to make it forwards-compatible with the zend-stdlib and
  zend-servicemanager v3 versions.
- [#4](https://github.com/zendframework/zend-console/pull/4) fixes an error in
  `getTitle()` whereby the `$output` array was being incorrectly used as a
  string.
- [#12](https://github.com/zendframework/zend-console/pull/12) updates the
  `Zend\Console\Prompt\Char::show()` method to call on the composed adapter's
  `write()`/`writeLine()` methods instead of calling `echo()`.

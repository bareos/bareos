# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.1 - 2016-02-08

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-filter#24](https://github.com/zendframework/zend-filter/pull/24) updates the
  `FilterPluginManager` to reference the `NumberFormat` **filter**, instead of
  the **view helper**.

## 2.6.0 - 2016-02-04

### Added

- [zendframework/zend-filter#14](https://github.com/zendframework/zend-filter/pull/14) adds the
  `UpperCaseWords` filter to the default list of filters known to the
  `FilterPluginManager`.
- [zendframework/zend-filter#22](https://github.com/zendframework/zend-filter/pull/22) adds
  documentation, and automatically publishes it to
  https://docs.laminas.dev/laminas-filter/

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-filter#15](https://github.com/zendframework/zend-filter/pull/15),
  [zendframework/zend-filter#19](https://github.com/zendframework/zend-filter/pull/19), and
  [zendframework/zend-filter#21](https://github.com/zendframework/zend-filter/pull/21)
  update the component to be forwards-compatible with laminas-servicemanager v3,
  and reduce the number of development dependencies required for testing.

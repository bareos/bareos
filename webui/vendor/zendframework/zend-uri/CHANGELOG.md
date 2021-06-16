# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.7.1 - 2019-10-07

### Added

- Nothing.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#34](https://github.com/zendframework/zend-uri/pull/34) fixes hostname recognition
  when port number is not provided. Additional colon is stripped out.

## 2.7.0 - 2019-02-27

### Added

- Nothing.

### Changed

- [#29](https://github.com/zendframework/zend-uri/pull/29) changes the behavior of `getHost()`:
  it will now always return a lowercase representation. This is in accord with
  [IETF 3986 Section 3.2.2](https://tools.ietf.org/html/rfc3986#section-3.2.2).

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.6.2 - 2019-02-26

### Added

- [#28](https://github.com/zendframework/zend-uri/pull/28) adds support for PHP 7.3.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.6.1 - 2018-04-30

### Added

- Nothing.

### Changed

- [#23](https://github.com/zendframework/zend-uri/pull/23) updates the zend-validator dependency to the 2.10 series, in order to ensure that
  this package can run under PHP 7.2.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.6.0 - 2018-04-10

### Added

- [#4](https://github.com/zendframework/zend-uri/pull/4) adds and publishes the
  documentation to https://zendframework.github.io/zend-uri/

### Deprecated

- Nothing.

### Removed

- [#16](https://github.com/zendframework/zend-uri/pull/16) removes support for
  PHP 5.5.

- [#16](https://github.com/zendframework/zend-uri/pull/16) removes support for
  HHVM.

### Fixed

- [#17](https://github.com/zendframework/zend-uri/pull/17) updates the path
  encoding algorithm to allow `(` and `)` characters as path characters (per
  the RFC-3986, these are valid sub-delimiters allowed within a path).

## 2.5.2 - 2016-02-17

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#3](https://github.com/zendframework/zend-uri/pull/3) updates dependencies to
  allow the component to work with both PHP 5 and PHP 7 versions, as well as
  all 2.X versions of required Zend components.

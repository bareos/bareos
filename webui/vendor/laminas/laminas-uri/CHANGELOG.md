# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.2 - 2019-02-26

### Added

- [zendframework/zend-uri#28](https://github.com/zendframework/zend-uri/pull/28) adds support for PHP 7.3.

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

- [zendframework/zend-uri#23](https://github.com/zendframework/zend-uri/pull/23) updates the laminas-validator dependency to the 2.10 series, in order to ensure that
  this package can run under PHP 7.2.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.6.0 - 2018-04-10

### Added

- [zendframework/zend-uri#4](https://github.com/zendframework/zend-uri/pull/4) adds and publishes the
  documentation to https://docs.laminas.dev/laminas-uri/

### Deprecated

- Nothing.

### Removed

- [zendframework/zend-uri#16](https://github.com/zendframework/zend-uri/pull/16) removes support for
  PHP 5.5.

- [zendframework/zend-uri#16](https://github.com/zendframework/zend-uri/pull/16) removes support for
  HHVM.

### Fixed

- [zendframework/zend-uri#17](https://github.com/zendframework/zend-uri/pull/17) updates the path
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

- [zendframework/zend-uri#3](https://github.com/zendframework/zend-uri/pull/3) updates dependencies to
  allow the component to work with both PHP 5 and PHP 7 versions, as well as
  all 2.X versions of required Laminas components.

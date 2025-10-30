# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.0 - 2015-09-22

### Added

- [zendframework/zend-form#17](https://github.com/zendframework/zend-form/pull/17) updates the component
  to use laminas-hydrator for hydrator functionality; this provides forward
  compatibility with laminas-hydrator, and backwards compatibility with
  hydrators from older versions of laminas-stdlib.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.5.3 - 2015-09-22

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-form#16](https://github.com/zendframework/zend-form/pull/16) updates the
  laminas-stdlib dependency to reference `>=2.5.0,<2.7.0` to ensure hydrators
  will work as expected following extraction of hydrators to the laminas-hydrator
  repository.

## 2.5.2 - 2015-09-09

### Added

- Nothing.

### Deprecated

- [zendframework/zend-form#12](https://github.com/zendframework/zend-form/pull/12) deprecates the
  `AllowEmpty` and `ContinueIfEmpty` annotations, to mirror changes made in
  [laminas-inputfilter#26](https://github.com/zendframework/zend-inputfilter/pull/26).

### Removed

- Nothing.

### Fixed

- [zendframework/zend-form#1](https://github.com/zendframework/zend-form/pull/1) `AbstractHelper` was
  being utilized on the method signature vs. `HelperInterface`.
- [zendframework/zend-form#9](https://github.com/zendframework/zend-form/pull/9) fixes typos in two
  `aria` attribute names in the `AbstractHelper`.

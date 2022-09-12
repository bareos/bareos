# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.0 - 2015-09-22

### Added

- [#17](https://github.com/zendframework/zend-form/pull/17) updates the component
  to use zend-hydrator for hydrator functionality; this provides forward
  compatibility with zend-hydrator, and backwards compatibility with
  hydrators from older versions of zend-stdlib.

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

- [#16](https://github.com/zendframework/zend-form/pull/16) updates the
  zend-stdlib dependency to reference `>=2.5.0,<2.7.0` to ensure hydrators
  will work as expected following extraction of hydrators to the zend-hydrator
  repository.

## 2.5.2 - 2015-09-09

### Added

- Nothing.

### Deprecated

- [#12](https://github.com/zendframework/zend-form/pull/12) deprecates the
  `AllowEmpty` and `ContinueIfEmpty` annotations, to mirror changes made in
  [zend-inputfilter#26](https://github.com/zendframework/zend-inputfilter/pull/26).

### Removed

- Nothing.

### Fixed

- [#1](https://github.com/zendframework/zend-form/pull/1) `AbstractHelper` was
  being utilized on the method signature vs. `HelperInterface`.
- [#9](https://github.com/zendframework/zend-form/pull/9) fixes typos in two
  `aria` attribute names in the `AbstractHelper`.

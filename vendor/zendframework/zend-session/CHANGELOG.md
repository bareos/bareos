# Changelog

All notable changes post-2.4 to this project will be documented in this file, in reverse chronological order by release.

## 2.4.12 - 2017-06-19

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Provides a fix to support PHP 7.1 and higher. Previously, the signature of
  `AbstractContainer::offsetGet` did not return by reference. Because the class
  from which it inherits, `Zend\Stdlib\ArrayObject`, _does_ return by reference,
  the signature change was considered invalid under PHP 7.1+. This patch updates
  the signature to be consistent with the zend-stdlib class.

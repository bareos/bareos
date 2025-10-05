# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 1.1.0 - 2016-02-18

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-hydrator#24](https://github.com/zendframework/zend-hydrator/pull/24) updates the code
  to be forwards-compatible with the v3 releases of laminas-stdlib,
  laminas-eventmanager, and laminas-servicemanager.

## 1.0.0 - 2015-09-17

Initial release. This ports all hydrator classes and functionality from
[laminas-stdlib](https://github.com/laminas/laminas-stdlib) to a standalone
repository. All final keywords are removed, to allow a deprecation cycle in the
laminas-stdlib component.

Please note: the following classes will be marked as `final` for a version 2.0.0
release to immediately follow 1.0.0:

- `Laminas\Hydrator\NamingStrategy\IdentityNamingStrategy`
- `Laminas\Hydrator\NamingStrategy\ArrayMapNamingStrategy`
- `Laminas\Hydrator\NamingStrategy\CompositeNamingStrategy`
- `Laminas\Hydrator\Strategy\ExplodeStrategy`
- `Laminas\Hydrator\Strategy\StrategyChain`
- `Laminas\Hydrator\Strategy\DateTimeFormatterStrategy`
- `Laminas\Hydrator\Strategy\BooleanStrategy`

As such, you should not extend them.

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.1 - 2016-02-12

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-cache#73](https://github.com/zendframework/zend-cache/pull/73) fixes how the
  `EventManager` instance is lazy-instantiated in
  `Laminas\Cache\Storage\Adapter\AbstractAdapter::getEventManager()`. In 2.6.0, it
  was using the v3-specific syntax; it now uses syntax compatible with both v2
  and v3.

## 2.6.0 - 2016-02-11

### Added

- [zendframework/zend-cache#70](https://github.com/zendframework/zend-cache/pull/70) adds, revises, and
  publishes the documentation to https://docs.laminas.dev/laminas-cache/

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-cache#22](https://github.com/zendframework/zend-cache/pull/22),
  [zendframework/zend-cache#64](https://github.com/zendframework/zend-cache/pull/64),
  [zendframework/zend-cache#68](https://github.com/zendframework/zend-cache/pull/68), and
  [zendframework/zend-cache#69](https://github.com/zendframework/zend-cache/pull/69) update the
  component to be forwards-compatible with laminas-eventmanager,
  laminas-servicemanager, and laminas-stdlib v3.
- [zendframework/zend-cache#31](https://github.com/zendframework/zend-cache/issues/31)
  Check Documentation Code Blocks
- [zendframework/zend-cache#53](https://github.com/zendframework/zend-cache/pull/53)
  fixed seg fault in redis adapter on PHP 7
- [zendframework/zend-cache#50](https://github.com/zendframework/zend-cache/issues/50)
  fixed APC tests not running on travis-ci since apcu-5 was released
- [zendframework/zend-cache#36](https://github.com/zendframework/zend-cache/pull/36)
  fixed AbstractAdapter::internalDecrementItems
- [zendframework/zend-cache#38](https://github.com/zendframework/zend-cache/pull/38)
  better test coverage of AbstractAdapter
- [zendframework/zend-cache#45](https://github.com/zendframework/zend-cache/pull/45)
  removed unused internal function Filesystem::readInfoFile
- [zendframework/zend-cache#25](https://github.com/zendframework/zend-cache/pull/25)
  MongoDd: fixed expiration support and removed duplicated tests
- [zendframework/zend-cache#40](https://github.com/zendframework/zend-cache/pull/40)
  Fixed TTL support of `Redis::addItem`
- [zendframework/zend-cache#18](https://github.com/zendframework/zend-cache/issues/18)
  Fixed `Redis::getCapabilities` and `RedisResourceManager::getMajorVersion`
  if resource wasn't initialized before

## 2.5.3 - 2015-09-15

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-cache#15](https://github.com/zendframework/zend-cache/pull/15) fixes an issue
  observed on HHVM when merging a list of memcached servers to add to the
  storage resource.
- [zendframework/zend-cache#17](https://github.com/zendframework/zend-cache/pull/17) Composer: moved
  `laminas/laminas-serializer` from `require` to `require-dev` as using the
  serializer is optional.
- A fix was provided for [ZF2015-07](https://getlaminas.org/security/advisory/ZF2015-07),
  ensuring that any directories or files created by the component use umask 0002
  in order to prevent arbitrary local execution and/or local privilege
  escalation.

## 2.5.2 - 2015-07-16

### Added

- [zendframework/zend-cache#10](https://github.com/zendframework/zend-cache/pull/10) adds TTL support
  for the Redis adapter.
- [zendframework/zend-cache#6](https://github.com/zendframework/zend-cache/pull/6) adds more suggestions
  to the `composer.json` for PHP extensions supported by storage adapters.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-cache#9](https://github.com/zendframework/zend-cache/pull/9) fixes an issue when
  connecting to a Redis instance with the `persistent_id` option.

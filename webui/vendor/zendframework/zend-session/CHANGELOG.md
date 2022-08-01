# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.7.4 - 2017-06-19

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#66](https://github.com/zendframework/zend-session/pull/66) fixes how the
  `Cache` save handler's `destroy()` method works, ensuring it does not attempt
  to remove an item by `$id` if it does not already exist in the cache.
- [#79](https://github.com/zendframework/zend-session/pull/79) updates the
  signature of `AbstractContainer::offsetGet()` to match
  `Zend\Stdlib\ArrayObject` and return by reference, fixing an issue when
  running under PHP 7.1+.

## 2.7.3 - 2016-07-05

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#51](https://github.com/zendframework/zend-session/pull/51) provides a fix to
  the `DbTableGateway` save handler to prevent infinite recursion when
  attempting to destroy an expired record during initial read operations.
- [#45](https://github.com/zendframework/zend-session/pull/45) updates the
  `SessionManager::regenerateId()` method to only regenerate the identifier if a
  session already exists.

## 2.7.2 - 2016-06-24

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#46](https://github.com/zendframework/zend-session/pull/46) provides fixes to
  each of the `Cache` and `DbTaleGateway` save handlers to ensure they work
  when used under PHP 7.

## 2.7.1 - 2016-05-11

### Added

- [#40](https://github.com/zendframework/zend-session/pull/40) adds and
  publishes the documentation to https://zendframework.github.io/zend-session/

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#38](https://github.com/zendframework/zend-session/pull/38) ensures that the
  value from `session.gc_maxlifetime` is cast to an integer before assigning
  it as the `lifetime` value in the `MongoDB` adapter, ensuring sessions may be
  deleted.

## 2.7.0 - 2016-04-12

### Added

- [#23](https://github.com/zendframework/zend-session/pull/23) provides a new
  `Id` validator to ensure that the session identifier is not malformed. This
  validator is now enabled by default; to disable it, pass
  `['attach_default_validators' => false]` as the fifth argument to
  `SessionManager`, or pass an `options` array with that value under the
  `session_manager` configuration key.
- [#34](https://github.com/zendframework/zend-session/pull/34) adds the option
  to use `exporeAfterSeconds` with the `MongoDB` save handler.
- [#37](https://github.com/zendframework/zend-session/pull/37) exposes the
  package as a standalone config-provider/component, adding:
  - `Zend\Session\ConfigProvider`, which maps the default services offered by
    the package, including the `ContainerAbstractServiceFactory`.
  - `Zend\Session\Module`, which does the same, but for zend-mvc contexts.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#34](https://github.com/zendframework/zend-session/pull/34) updates the
  component to use ext/mongodb + the MongoDB PHP client library, instead of
  ext/mongo, for purposes of the `MongoDB` save handler, allowing the component
  to be used with modern MongoDB installations.

## 2.6.2 - 2016-02-25

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#32](https://github.com/zendframework/zend-session/pull/32) provides a better
  polfill for the `ValidatorChain` to ensure it can be represented in
  auto-generated classmaps (e.g., via `composer dump-autoload --optimize` and/or
  `composer dump-autoload --classmap-authoritative`).

## 2.6.1 - 2016-02-23

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#29](https://github.com/zendframework/zend-session/pull/29) extracts the
  constructor defined in `Zend\Session\Validator\ValidatorChainTrait` and pushes
  it into each of the `ValidatorChainEM2` and `ValidatorChainEM3`
  implementations, to prevent colliding constructor definitions due to
  inheritance + trait usage.

## 2.6.0 - 2016-02-23

### Added

- [#29](https://github.com/zendframework/zend-session/pull/29) adds two new
  classes: `Zend\Session\Validator\ValidatorChainEM2` and `ValidatorChainEM3`.
  Due to differences in the `EventManagerInterface::attach()` method between
  zend-eventmanager v2 and v3, and the fact that `ValidatorChain` overrides that
  method, we now need an implementation targeting each major version. To provide
  a consistent use case, we use a polyfill that aliases the appropriate version
  to the `Zend\Session\ValidatorChain` class.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#29](https://github.com/zendframework/zend-session/pull/29) updates the code
  to be forwards compatible with the v3 releases of zend-eventmanager and
  zend-servicemanager.
- [#7](https://github.com/zendframework/zend-session/pull/7) Mongo save handler
  was using sprintf formatting without sprintf.

## 2.5.2 - 2015-07-29

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#3](https://github.com/zendframework/zend-session/pull/3) Utilize
  SaveHandlerInterface vs. our own.

- [#2](https://github.com/zendframework/zend-session/pull/2) detect session
  exists by use of *PHP_SESSION_ACTIVE*

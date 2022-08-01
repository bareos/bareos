# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.3 - 2016-02-23

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#74](https://github.com/zendframework/zend-mvc/pull/74) fixes the
  `FormAnnotationBuilderFactory`'s usage of the
  `FormElementManager::injectFactory()` method to ensure it works correctly on
  all versions.

## 2.6.2 - 2016-02-22

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#71](https://github.com/zendframework/zend-mvc/pull/71) fixes the
  `ViewHelperManagerFactory` to be backwards-compatible with v2 by ensuring that
  the factories for each of the `url`, `basepath`, and `doctype` view helpers
  are registered using the fully qualified class names present in
  `Zend\View\HelperPluginManager`; these changes ensure requests for these
  helpers resolve to these override factories, instead of the
  `InvokableFactory`.

## 2.6.1 - 2016-02-16

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#69](https://github.com/zendframework/zend-mvc/pull/69) largely reverts
  [#30](https://github.com/zendframework/zend-mvc/pull/30), having the component
  utilize the `HydratorPluginManager` from zend-stdlib 2.7.5. This was done to
  provide backwards compatibility; while zend-stdlib Hydrator types can be used
  in place of zend-hydrator types, the reverse is not true.

  You can make your code forwards-compatible with version 3, where the
  `HydratorPluginManager` will be pulled from zend-hydrator, by updating your
  typehints to use the zend-hydrator classes instead of those from zend-stdlib;
  the instances returned from the zend-stdlib `HydratorPluginManager`, because
  they extend those from zend-hydrator, remain compatible. 

## 2.6.0 - 2015-09-22

### Added

- [#30](https://github.com/zendframework/zend-mvc/pull/30) updates the component
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

- [#29](https://github.com/zendframework/zend-mvc/pull/29) updates the
  zend-stdlib dependency to reference `>=2.5.0,<2.7.0` to ensure hydrators
  will work as expected following extraction of hydrators to the zend-hydrator
  repository.

## 2.5.2 - 2015-09-14

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#27](https://github.com/zendframework/zend-mvc/pull/27) fixes a condition
  where non-view model results from controllers could cause errors to be
  raisedin the `DefaultRenderingStrategy`.

# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.1 - 2016-04-07

### Added

- [zendframework/zend-inputfilter#68](https://github.com/zendframework/zend-inputfilter/pull/68) adds support
  for using *either* named keys *or* a `name` element in input filter specs
  parsed by the `InputFilterAbstractServiceFactory`.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#67](https://github.com/zendframework/zend-inputfilter/pull/67) and
  [zendframework/zend-inputfilter#73](https://github.com/zendframework/zend-inputfilter/pull/73) fix
  localization of the `NotEmpty` validation error message (created for any
  required input for which a value was not provided).

## 2.6.0 - 2016-02-18

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#86](https://github.com/zendframework/zend-inputfilter/pull/86),
  [zendframework/zend-inputfilter#95](https://github.com/zendframework/zend-inputfilter/pull/95), and
  [zendframework/zend-inputfilter#96](https://github.com/zendframework/zend-inputfilter/pull/96) update the
  component to be forwards-compatible with laminas-servicemanager v3.
- [zendframework/zend-inputfilter#72](https://github.com/zendframework/zend-inputfilter/pull/72) `ArrayInput`
  value is properly reset after `BaseInputFilter::setData()`

## 2.5.5 - 2015-09-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#22](https://github.com/zendframework/zend-inputfilter/pull/22) adds tests to
  verify two conditions around inputs with fallback values:
  - If the input was not in the data set, it should not be represented in either
    the list of valid *or* invalid inputs.
  - If the input *was* in the data set, but empty, it should be represented in
    the list of valid inputs.
- [zendframework/zend-inputfilter#31](https://github.com/zendframework/zend-inputfilter/pull/31) updates the
  `InputFilterInterface::add()` docblock to match existing, shipped implementations.
- [zendframework/zend-inputfilter#25](https://github.com/zendframework/zend-inputfilter/pull/25) updates the
  input filter to prevent validation of missing optional fields (a BC break
  since 2.3.9). This change likely requires changes to your inputs as follows:

  ```php
  $input = new Input();
  $input->setAllowEmpty(true);         // Disable BC Break logic related to treat `null` values as valid empty value instead *not set*.
  $input->setContinueIfEmpty(true);    // Disable BC Break logic related to treat `null` values as valid empty value instead *not set*.
  $input->getValidatorChain()->attach(
      new Laminas\Validator\NotEmpty(),
      true                             // break chain on failure

  );
  ```

  ```php
  $inputSpecification = [
    'allow_empty'       => true,
    'continue_if_empty' => true,
    'validators' => [
      [
        'break_chain_on_failure' => true,
        'name'                   => 'Laminas\\Validator\\NotEmpty',
      ],
    ],
  ];
  ```
- [Numerous fixes](https://github.com/laminas/laminas-inputfilter/milestones/2.4.8)
  aimed at bringing the functionality back to the pre-2.4 code, and improving
  quality overall of the component via increased testing and test coverage.

## 2.5.4 - 2015-08-11

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#15](https://github.com/zendframework/zend-inputfilter/pull/15) ensures that
  `ArrayAccess` data provided to an input filter using `setData()` can be
  validated, a scenario that broke with [zendframework/zend-inputfilter#7](https://github.com/zendframework/zend-inputfilter/pull/7).

## 2.5.3 - 2015-08-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#10](https://github.com/zendframework/zend-inputfilter/pull/10) fixes an
  issue with with the combination of `required`, `allow_empty`, and presence of
  a fallback value on an input introduced in 2.4.5. Prior to the fix, the
  fallback value was no longer considered when the value was required but no
  value was provided; it now is.

## 2.5.2 - 2015-07-28

### Added

- [zendframework/zend-inputfilter#2](https://github.com/zendframework/zend-inputfilter/pull/2) adds support
  in `Laminas\InputFilter\Factory` for using the composed `InputFilterManager` to
  retrieve an input of a given `type` based on configuration; only if the type
  is not available in the factory will it attempt to directly instantiate it.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#7](https://github.com/zendframework/zend-inputfilter/pull/7) fixes an issue
  with the combination of `required` and `allow_empty`, now properly
  invalidating a data set if the `required` input is missing entirely
  (previously, it would consider the data set valid, and auto-initialize the
  missing input to `null`).

## 2.4.8 - TBD

### Added

- Nothing.

### Deprecated

- [zendframework/zend-inputfilter#26](https://github.com/zendframework/zend-inputfilter/pull/26) Deprecate magic logic for auto attach a NonEmpty
 validator with breakChainOnFailure = true. Instead append NonEmpty validator when desired.

  ```php
  $input = new Laminas\InputFilter\Input();
  $input->setContinueIfEmpty(true);
  $input->setAllowEmpty(true);
  $input->getValidatorChain()->attach(new Laminas\Validator\NotEmpty(), /* break chain on failure */ true);
  ```

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.4.7 - 2015-08-11

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#15](https://github.com/zendframework/zend-inputfilter/pull/15) ensures that
  `ArrayAccess` data provided to an input filter using `setData()` can be
  validated, a scenario that broke with [zendframework/zend-inputfilter#7](https://github.com/zendframework/zend-inputfilter/pull/7).

## 2.4.6 - 2015-08-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#10](https://github.com/zendframework/zend-inputfilter/pull/10) fixes an
  issue with with the combination of `required`, `allow_empty`, and presence of
  a fallback value on an input introduced in 2.4.5. Prior to the fix, the
  fallback value was no longer considered when the value was required but no
  value was provided; it now is.

## 2.4.5 - 2015-07-28

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [zendframework/zend-inputfilter#7](https://github.com/zendframework/zend-inputfilter/pull/7) fixes an issue
  with the combination of `required` and `allow_empty`, now properly
  invalidating a data set if the `required` input is missing entirely
  (previously, it would consider the data set valid, and auto-initialize the
  missing input to `null`).

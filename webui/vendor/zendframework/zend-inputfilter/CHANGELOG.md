# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.1 - 2016-04-07

### Added

- [#68](https://github.com/zendframework/zend-inputfilter/pull/68) adds support
  for using *either* named keys *or* a `name` element in input filter specs
  parsed by the `InputFilterAbstractServiceFactory`.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#67](https://github.com/zendframework/zend-inputfilter/pull/67) and
  [#73](https://github.com/zendframework/zend-inputfilter/pull/73) fix
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

- [#86](https://github.com/zendframework/zend-inputfilter/pull/86),
  [#95](https://github.com/zendframework/zend-inputfilter/pull/95), and
  [#96](https://github.com/zendframework/zend-inputfilter/pull/96) update the
  component to be forwards-compatible with zend-servicemanager v3.
- [#72](https://github.com/zendframework/zend-inputfilter/pull/72) `ArrayInput`
  value is properly reset after `BaseInputFilter::setData()`

## 2.5.5 - 2015-09-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#22](https://github.com/zendframework/zend-inputfilter/pull/22) adds tests to
  verify two conditions around inputs with fallback values:
  - If the input was not in the data set, it should not be represented in either
    the list of valid *or* invalid inputs.
  - If the input *was* in the data set, but empty, it should be represented in
    the list of valid inputs.
- [#31](https://github.com/zendframework/zend-inputfilter/pull/31) updates the
  `InputFilterInterface::add()` docblock to match existing, shipped implementations.
- [#25](https://github.com/zendframework/zend-inputfilter/pull/25) updates the
  input filter to prevent validation of missing optional fields (a BC break
  since 2.3.9). This change likely requires changes to your inputs as follows:

  ```php
  $input = new Input();
  $input->setAllowEmpty(true);         // Disable BC Break logic related to treat `null` values as valid empty value instead *not set*.
  $input->setContinueIfEmpty(true);    // Disable BC Break logic related to treat `null` values as valid empty value instead *not set*.
  $input->getValidatorChain()->attach(
      new Zend\Validator\NotEmpty(),
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
        'name'                   => 'Zend\\Validator\\NotEmpty',
      ],
    ],
  ];
  ```
- [Numerous fixes](https://github.com/zendframework/zend-inputfilter/milestones/2.4.8)
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

- [#15](https://github.com/zendframework/zend-inputfilter/pull/15) ensures that
  `ArrayAccess` data provided to an input filter using `setData()` can be
  validated, a scenario that broke with [#7](https://github.com/zendframework/zend-inputfilter/pull/7).

## 2.5.3 - 2015-08-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#10](https://github.com/zendframework/zend-inputfilter/pull/10) fixes an
  issue with with the combination of `required`, `allow_empty`, and presence of
  a fallback value on an input introduced in 2.4.5. Prior to the fix, the
  fallback value was no longer considered when the value was required but no
  value was provided; it now is.

## 2.5.2 - 2015-07-28

### Added

- [#2](https://github.com/zendframework/zend-inputfilter/pull/2) adds support
  in `Zend\InputFilter\Factory` for using the composed `InputFilterManager` to
  retrieve an input of a given `type` based on configuration; only if the type
  is not available in the factory will it attempt to directly instantiate it.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#7](https://github.com/zendframework/zend-inputfilter/pull/7) fixes an issue
  with the combination of `required` and `allow_empty`, now properly
  invalidating a data set if the `required` input is missing entirely
  (previously, it would consider the data set valid, and auto-initialize the
  missing input to `null`).

## 2.4.8 - TBD

### Added

- Nothing.

### Deprecated

- [#26](https://github.com/zendframework/zend-inputfilter/pull/26) Deprecate magic logic for auto attach a NonEmpty
 validator with breakChainOnFailure = true. Instead append NonEmpty validator when desired.

  ```php
  $input = new Zend\InputFilter\Input();
  $input->setContinueIfEmpty(true);
  $input->setAllowEmpty(true);
  $input->getValidatorChain()->attach(new Zend\Validator\NotEmpty(), /* break chain on failure */ true);
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

- [#15](https://github.com/zendframework/zend-inputfilter/pull/15) ensures that
  `ArrayAccess` data provided to an input filter using `setData()` can be
  validated, a scenario that broke with [#7](https://github.com/zendframework/zend-inputfilter/pull/7).

## 2.4.6 - 2015-08-03

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#10](https://github.com/zendframework/zend-inputfilter/pull/10) fixes an
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

- [#7](https://github.com/zendframework/zend-inputfilter/pull/7) fixes an issue
  with the combination of `required` and `allow_empty`, now properly
  invalidating a data set if the `required` input is missing entirely
  (previously, it would consider the data set valid, and auto-initialize the
  missing input to `null`).

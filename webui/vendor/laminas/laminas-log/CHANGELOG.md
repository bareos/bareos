# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.6.0 - 2015-07-20

### Added

- [zendframework/zend-log#6](https://github.com/zendframework/zend-log/pull/6) adds
  [PSR-3](http://www.php-fig.org/psr/psr-3/) support to laminas-log:
  - `Laminas\Log\PsrLoggerAdapter` allows you to decorate a
    `Laminas\Log\LoggerInterface` instance so it can be used wherever a PSR-3
    logger is expected.
  - `Laminas\Log\Writer\Psr` allows you to decorate a PSR-3 logger instance for use
    as a log writer with `Laminas\Log\Logger`.
  - `Laminas\Log\Processor\PsrPlaceholder` allows you to use PSR-3-compliant
    message placeholders in your log messages; they will be substituted from
    corresponding keys of values passed in the `$extra` array when logging the
    message.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.5.2 - 2015-07-06

### Added

- [zendframework/zend-log#2](https://github.com/zendframework/zend-log/pull/2) adds
  the ability to specify the mail transport via the configuration options for a
  mail log writer, using the same format supported by
  `Laminas\Mail\Transport\Factory::create()`; as an example:

  ```php
  $writer = new MailWriter([
      'mail' => [
          // message options
      ],
      'transport' => [
          'type' => 'smtp',
          'options' => [
               'host' => 'localhost',
          ],
      ],
  ]);
  ```

### Deprecated

- Nothing.

### Removed

- [zendframework/zend-log#43](https://github.com/zendframework/zend-diactoros/pull/43) removed both
  `ServerRequestFactory::marshalUri()` and `ServerRequestFactory::marshalHostAndPort()`,
  which were deprecated prior to the 1.0 release.

### Fixed

- [zendframework/zend-log#4](https://github.com/zendframework/zend-log/pull/4) adds better, more
  complete verbiage to the `composer.json` `suggest` section, to detail why
  and when you might need additional dependencies.
- [zendframework/zend-log#1](https://github.com/zendframework/zend-log/pull/1) updates the code to
  remove conditionals related to PHP versions prior to PHP 5.5, and use bound
  closures in tests (not possible before 5.5).

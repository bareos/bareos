# Changelog

All notable changes to this project will be documented in this file, in reverse chronological order by release.

## 2.11.4 - 2019-12-04

### Added

- Nothing.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#203](https://github.com/zendframework/zend-view/pull/203) fixes PHP 7.4 compatibility.

## 2.11.3 - 2019-10-11

### Added

- Nothing.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#192](https://github.com/zendframework/zend-view/pull/192) changes
  curly braces in array and string offset access to square brackets
  in order to prevent issues under the upcoming PHP 7.4 release.

- [#195](https://github.com/zendframework/zend-view/pull/195) fixes PHP 7.4 compatibility.

## 2.11.2 - 2019-02-19

### Added

- Nothing.

### Changed

- [#180](https://github.com/zendframework/zend-view/pull/180) alters the behavior of the `HeadMeta` helper to honor the value of the
  `autoEscape` flag when rendering values. Previously, it would ignore the
  setting and always escape the values.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.11.1 - 2018-12-10

### Added

- [#179](https://github.com/zendframework/zend-view/pull/179) adds `^3.0` to the zendframework/zend-json constraints, allowing v3
  releases to be used with zend-view.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.11.0 - 2018-12-06

### Added

- [#168](https://github.com/zendframework/zend-view/pull/168) adds two new methods to `Zend\View\Helper\Placeholder` (and thus any
  helper extending it):

  - `deleteContainer(string $name)` can be used to delete a placeholder container.
  - `clearContainers()` can be used to clear all placeholder containers.

  These new features are particularly useful when in long-running server
  environments, such as Swoole, where you may need to clear the contents on each
  request.

### Changed

- [#155](https://github.com/zendframework/zend-view/pull/155) modifies the `Zend\View\Helper\Service\IdentifyFactory` such that it will
  now also look for the service `Zend\Authentication\AuthenticationServiceInterface`
  if the service `Zend\Authentication\AuthenticationService` is not found. This
  allows using a service named after the interface instead of the
  implementation if desired.

- [#158](https://github.com/zendframework/zend-view/pull/158) modifies how a `ViewModel` (and all extensions) is cloned; the `$variables`
  property, if it is an object, is now cloned as well to ensure changes in the
  new instance do not affect the current one.

- [#153](https://github.com/zendframework/zend-view/pull/153) updates the `ConsoleModel::setErrorLevel()` method to implement a fluent
  interface.

### Deprecated

- Nothing.

### Removed

- [#147](https://github.com/zendframework/zend-view/pull/147) removes the property `$regKey` from a number of helpers; these were a
  remnant of ZF1, and have not been used internally since the initial 2.0.0
  release.

### Fixed

- [#164](https://github.com/zendframework/zend-view/pull/164) fixes the various `Head*` view helpers such that they will now properly
  escape attributes as HTML attributes (instead of as HTML content).

## 2.10.1 - 2018-12-06

### Added

- [#173](https://github.com/zendframework/zend-view/pull/173) adds support for PHP 7.3.

### Changed

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#170](https://github.com/zendframework/zend-view/pull/170) ensures that variables referenced in `compact()` operations are properly
  initialized, fixing an error that occures in PHP 7.3.

- [#169](https://github.com/zendframework/zend-view/pull/169) adds zendframework/zend-json as a required dependency, as it is referenced
  in multiple locations within the package.

- [#156](https://github.com/zendframework/zend-view/pull/156) adds missing `@method` annotations to the `HeadMeta` helper.

## 2.10.0 - 2018-01-17

### Added

- [#135](https://github.com/zendframework/zend-view/pull/135) adds support
  for PHP 7.2.

- [#138](https://github.com/zendframework/zend-view/pull/138) adds support for
  the HTML5 "as" attribute to the `HeadLink` helper. This can be used to help
  prioritize resource loading.

- [#139](https://github.com/zendframework/zend-view/pull/139) adds two new
  methods to the `Zend\View\Helper\Gravatar` class: `setAttributes()` and
  `getAttributes()`.

### Changed

- [#133](https://github.com/zendframework/zend-view/pull/133) modifies the
  behavior the `placeholder()` helper to no longer render a prefix or postfix if
  no items are available in the container.

### Deprecated

- [#139](https://github.com/zendframework/zend-view/pull/139) deprecates the
  `Zend\View\Helper\Gravatar` methods `setAttribs()` and `getAttribs()` in favor
  of the new methods `setAttributes()` and `getAttributes()`, respectively.

### Removed

- [#135](https://github.com/zendframework/zend-view/pull/135) removes support
  for HHVM.

### Fixed

- Nothing.

## 2.9.1 - 2018-01-17

### Added

- [#136](https://github.com/zendframework/zend-view/pull/136) updates the
  `Navigation` helper class to document the various proxy methods it allows via
  method overloading via `@method` annotations.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#123](https://github.com/zendframework/zend-view/pull/123) updates the
  `HelperPluginManager` such that it no longer injects a translator in a helper
  if one is already present.

- [#125](https://github.com/zendframework/zend-view/pull/125) provides an update
  to the `PhpRenderer:render()` method such that it will now catch not only
  `Exception` instances, but also PHP 7 `Throwable` instances, and properly
  cleanup the output buffers when it does.

- [#121](https://github.com/zendframework/zend-view/pull/121) provides a fix to
  ensure that content generated on a previous execution of `PhpRenderer::render()`
  is never re-used.

## 2.9.0 - 2017-03-21

### Added

- [#89](https://github.com/zendframework/zend-view/pull/89) updates the
  `HeadScript` and `InlineScript` view helpers to whitelist the `id` attribute
  as an optional attribute.

- [#96](https://github.com/zendframework/zend-view/pull/96) updates the
  `HeadScript`, `HeadLink`, and `InlineScript` view helpers to whitelist the
  `crossorigin` and `integrity` attributes as optional attributes.

- [#64](https://github.com/zendframework/zend-view/pull/64) adds a new `Asset`
  view helper. This helper uses the following configuration to map a named asset
  to the actual file to serve:

  ```php
  'view_helper_config' => [
      'asset' => [
          'resource_map' => [
              'css/style.css' => 'css/style-3a97ff4ee3.css',
              'js/vendor.js' => 'js/vendor-a507086eba.js',
          ],
      ],
  ],
  ```

  This can also be automated via tools such as gulp-rev and grunt-rev by using
  the `rev-manifest.json` each creates directly within your configuration:

  ```php
  'view_helper_config' => [
      'asset' => [
          'resource_map' => json_decode(file_get_contents('path/to/rev-manifest.json'), true),
      ],
  ],
  ```

  The benefit of this approach is that it allows your view scripts to reference
  a static asset name, while integrating with your JS and CSS build tools.

### Deprecated

- Nothing.

### Removed

- [#114](https://github.com/zendframework/zend-view/pull/114) removes support
  for PHP 5.5.

### Fixed

- [#110](https://github.com/zendframework/zend-view/pull/110) provides a fix
  for the navigation helpers to ensure that usage of either the `default` or
  `navigation` containers (documentation specified `default`, but usage only
  allowed `navigation` previously). When `default` is specified, the
  `Zend\Navigation\Navigation` service will be used for the container; if
  `navigation` is used, that service will be pulled instead (which is usually an
  alias for the `Zend\Navigation\Navigation` service anyways).

## 2.8.2 - 2017-03-20

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#92](https://github.com/zendframework/zend-view/pull/92) fixes the docblocks
  and exception messages in the breadcrumbs and menu navigation helpers to
  remove references to 'module' keys for the `$partial` argument, as that key
  is no longer used.
- [#98](https://github.com/zendframework/zend-view/pull/98) fixes how the
  `HeadMeta` helper renders the `<meta charset>` tag, ensuring it is the first
  rendered. As long as the `HeadMeta` helper is called early in your markup, this
  should ensure it is within the first 1024 characters, ensuring your document
  validates.
- [#104](https://github.com/zendframework/zend-view/pull/104) fixes the
  `@method` annotation for the `Placeholder` view helper to use the correct case,
  fixing issues with method completion in IDEs.
- [#112](https://github.com/zendframework/zend-view/pull/112) fixes an issue in
  the `PhpRendererStrategy` whereby absence of a response instance in the
  `ViewEvent` would lead to a fatal error.

## 2.8.1 - 2016-06-30

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#78](https://github.com/zendframework/zend-view/pull/78) and
  [#79](https://github.com/zendframework/zend-view/pull/79) ensure that all
  helpers work with both version 2 and version 3 of zend-mvc.

## 2.8.0 - 2016-06-21

### Added

- [#67](https://github.com/zendframework/zend-view/pull/67) adds a script,
  `templatemap_generator.php`, which is available in
  `vendor/bin/templatemap_generator.php` once installed. This script replaces
  the original present in the zendframework/zendframework package, and
  simplifies it for the most common use case. Usage is:

  ```bash
  $ cd module/ModuleName/config
  $ ../../../vendor/bin/templatemap_generator.php ../view > template_map.config.php
  ```

  You can also provide a list of files via globbing or usage of `find` after the
  initial directory argument; if provided that list of files will be used to
  generate the map. (The directory argument is then used to strip the path
  information when generating the template name.)

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.7.0 - 2016-05-12

### Added

- [#1](https://github.com/zendframework/zend-view/pull/1) adds a new `loop()`
  method to the `partialLoop()` helper, allowing the ability to chain setters
  with rendering:
  `$this->partialLoop()->setObjectKey('foo')->loop('partial', $data)`
- [#60](https://github.com/zendframework/zend-view/pull/60) adds the ability to
  register and consume arbitrary callables as view helpers within the
  `HelperPluginManager`.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- Nothing.

## 2.6.8 - 2016-05-12

### Added

- [#22](https://github.com/zendframework/zend-view/pull/22) adds support for the
  `async` attribute within the `headScript` helper.
- [#59](https://github.com/zendframework/zend-view/pull/59) adds and publishes
  the documentation to https://zendframework.github.io/zend-view/

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#21](https://github.com/zendframework/zend-view/pull/21) updates the
  `headScript` helper to allow empty attribute types to render as keys only when
  using an HTML5 doctype.

## 2.6.7 - 2016-04-18

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#58](https://github.com/zendframework/zend-view/pull/58) updates the `url()`
  helper so that it can work with either the zend-mvc v2 router subcomponent or
  zend-router.

## 2.6.6 - 2016-04-18

### Added

- [#57](https://github.com/zendframework/zend-view/pull/57) adds
  `Zend\View\Helper\TranslatorAwareTrait`, which provides implementation for
  `Zend\I18n\Translator\TranslatorAwareInterface`, and allowed removal of
  duplicated implementations in several helpers.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#57](https://github.com/zendframework/zend-view/pull/57) removes the explicit
  dependency on `Zend\I18n\Translator\TranslatorAwareInterface` by allowing
  helpers to duck type the interface to receive a translator during
  instantiation; this allows such helpers to work even when zend-i18n is not
  installed. The following helpers were updated to duck type the interface
  instead of implement it explicitly:
  - `FlashMessenger`
  - `HeadTitle`
  - all `Navigation` helpers

## 2.6.5 - 2016-03-21

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#55](https://github.com/zendframework/zend-view/pull/55) fixes a circular
  dependency issue in the navigation helpers with regards to event manager
  resolution.

## 2.6.4 - 2016-03-02

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#52](https://github.com/zendframework/zend-view/pull/52) fixes several issues
  detected after re-enabling tests skipped when executed against
  zend-servicemanager/zend-eventmanager v3:
  - `HelperPluginManager` now implements an `EventManagerAware` initializer.
  - `Zend\View\Helper\Navigation\AbstractHelper` now contains logic to ensure
    that when an `EventManager` instance is lazy-loaded, it composes a
    `SharedEventManager`.
  - The `FlashMessenger` factory now correctly pulls the `config` service, not
    the `Config` service (former is both backwards- and forwards compatible).

## 2.6.3 - 2016-02-22

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#50](https://github.com/zendframework/zend-view/pull/50) fixes
  the initializer defined and registered in
  `Navigation\PluginManager::__construct()` to ensure it properly pulls and
  injects the application container into navigation helpers, under both
  zend-servicemanager v2 and v3. Additionally, when lazy-instantiating the
  `Navigation\PluginManager`, the `Navigation` helper now passes the composed
  service manager instance to its constructor.

## 2.6.2 - 2016-02-18

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#47](https://github.com/zendframework/zend-view/pull/47) fixes
  `Navigation\PluginManager` to ensure it is backwards compatible
  with zend-servicemanager v2, including:
  - fixing the constructor to be BC with v2 and forwards-compatible with v3.
  - adding additional, normalized alias/factory pairs.
- [#47](https://github.com/zendframework/zend-view/pull/47) fixes
  the behavior of `HelperPluginManager::injectTranslator()` to return
  early if no container is provided (fixing an issue with navigation
  helpers introduced in 2.6.0).

## 2.6.1 - 2016-02-18

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#44](https://github.com/zendframework/zend-view/pull/44) fixes the
  constructor of `HelperPluginManager` to ensure it is backwards compatible
  with zend-servicemanager v2.

## 2.6.0 - 2016-02-17

### Added

- [#8](https://github.com/zendframework/zend-view/pull/8) adds a new method to
  each of the `Breadcrumbs` and `Menu` navigation helpers, 
  `renderPartialWithParams(array $params = [], $container = null, $partial = null)`.
  This method allows passing parameters to the navigation partial to render,
  just as you would when using the `partial()` view helper.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#15](https://github.com/zendframework/zend-view/pull/15),
  [#17](https://github.com/zendframework/zend-view/pull/17),
  [#35](https://github.com/zendframework/zend-view/pull/35), and
  [#42](https://github.com/zendframework/zend-view/pull/42) update the component
  to be forwards-compatible with the v3 releases of zend-eventmanager,
  zend-servicemanager, and zend-stdlib. The changes include:
  - changes to how events are triggered to ensure they continue working correctly.
  - updates to the plugin manager to be forwards-compatible.
  - updates to helper factories to be forwards-compatible.

## 2.5.3 - 2016-01-19

### Added

- [#5](https://github.com/zendframework/zend-view/pull/5) adds support for the
  `itemprop` attribute in the `headLink()` view helper.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#25](https://github.com/zendframework/zend-view/pull/25) updates
  `PhpRenderer::render()` to no longer lazy-instantiate a `FilterChain`;
  content filtering is now only done if a `FitlerChain` is already
  injected in the renderer.

## 2.5.2 - 2015-06-16

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#4](https://github.com/zendframework/zend-view/pull/4) fixes an issue with
  how the `ServerUrl` detects and emits the port when port-forwarding is in
  effect.

## 2.4.3 - 2015-06-16

### Added

- Nothing.

### Deprecated

- Nothing.

### Removed

- Nothing.

### Fixed

- [#4](https://github.com/zendframework/zend-view/pull/4) fixes an issue with
  how the `ServerUrl` detects and emits the port when port-forwarding is in
  effect.

# The StandardAutoloader

## Overview

`Zend\Loader\StandardAutoloader` is designed as a
[PSR-0](http://www.php-fig.org/psr/psr-0/)-compliant
autoloader. It assumes a 1:1 mapping of the namespace+classname to the
filesystem, wherein namespace separators and underscores are translated to
directory separators. The following statement illustrates how resolution works:

```php
$filename = str_replace(
    ['_', '\\'],
    DIRECTORY_SEPARATOR,
    $classname
) . '.php';
```

The `StandardAutoloader` requires that you explicitly register namespace/path
pairs (or vendor prefix/path pairs), and will only load a file if it exists
within the given path. Multiple pairs may be provided.

As a measure of last resort, you may also use the `StandardAutoloader` as a
"fallback" autoloader &mdash; one that will look for classes of any namespace or
vendor prefix on the `include_path`. This practice is not recommended, however,
due to performance implications.

Finally, as with all autoloaders in zend-loader, the `StandardAutoloader` is
capable of registering itself with PHP's SPL autoloader registry.

> ### Vocabulary: Namespaces vs. Vendor Prefixes
>
> In terms of autoloading, a "namespace" corresponds to PHP's own definition of
> namespaces.
>
> A "vendor prefix" refers to the practice, popularized in PHP versions prior to
> 5.3, of providing a pseudo-namespace in the form of underscore-separated words
> in class names. As an example, the class `Phly_Couch_Document` uses a vendor
> prefix of `Phly`, and a component prefix of `Phly_Couch`, but it is a class
> sitting in the global namespace.
>
> The `StandardAutoloader` is capable of loading either namespaced or vendor
> prefixed class names, but treats them separately when attempting to match them
> to an appropriate path.

## Quick Start

Basic use of the `StandardAutoloader` requires registering namespace/path pairs.
This can either be done at instantiation, or via explicit method calls after the
object has been initialized.  Calling `register()` will register the autoloader
with the SPL autoloader registry.

### Manual Configuration

```php
use Zend\Loader\StandardAutoloader;

// This example assumes the StandardAutoloader is autoloadable.
$loader = new StandardAutoloader();

// Register the "Phly" namespace:
$loader->registerNamespace('Phly', APPLICATION_PATH . '/../library/Phly');

// Register the "Scapi" vendor prefix:
$loader->registerPrefix('Scapi', APPLICATION_PATH . '/../library/Scapi');

// Optionally, specify the autoloader as a "fallback" autoloader;
// this is not recommended.
$loader->setFallbackAutoloader(true);

// Register with spl_autoload:
$loader->register();
```

### Configuration at Instantiation

The `StandardAutoloader` may also be configured at instantiation. Please note:

- The argument passed may be either an array or a `Traversable` object.
- The argument passed should also be a valid argument for passing to the
  `setOptions()` method.

The following is equivalent to the previous example.

```php
use Zend\Loader\StandardAutoloader;

$loader = new StandardAutoloader([
    'namespaces' => [
        'Phly' => APPLICATION_PATH . '/../library/Phly',
    ],
    'prefixes' => [
        'Scapi' => APPLICATION_PATH . '/../library/Scapi',
    ],
    'fallback_autoloader' => true,
]);

// Register with spl_autoload:
$loader->register();
```

## Configuration Options

The `StandardAutoloader` defines the following options.

### namespaces

An associative array of namespace/path pairs. The path should be an absolute
path or path relative to the calling script, and contain only classes that live
in that namespace (or its subnamespaces).

### prefixes

An associative array of vendor prefix/path pairs. The path should be an absolute
path or path relative to the calling script, and contain only classes that begin
with the provided vendor prefix.

### fallback_autoloader

A boolean value indicating whether or not this instance should act as a
"fallback" autoloader (i.e., look for classes of any namespace or vendor prefix
on the `include_path`). By default, `false`.

> ### autoregister_zf is deprecated
>
> One other option is available to the `StandardAutoloader`: `autoregister_zf`.
> We do not document it any longer, as it is no longer relevant.
>
> Starting with the 2.5.0 release of Zend Framework, the framework package
> itself is a "metapackage", defining only a `composer.json` file listing
> the packages for each component.
>
> As such, there is no single path in which all ZF files live, making the
> `autoregister_zf` flag useless for versions starting with 2.5.0; it will
> only register the zend-loader path!
>
> If you are using this feature, you should update your code. We recommend
> using [Composer](https://getcomposer.org)'s autoloader for autoloading
> Zend Framework classes.

## Available Methods

### \_\_construct

```php
__construct(array|Traversable $options = null) : void
```

Create a new instance of the object.

If `$options` is non-null, the argument is passed to
[setOptions()](#setoptions).

### setOptions

```php
setOptions(array|Traversable $options) : void
```

Set object state based on provided options.

Recognized keys are detailed under [Configuration options](#configuration-options),
with the following behaviors:

- The `namespaces` value will be passed to
  [registerNamespaces()](#registernamespaces).
- The `prefixes` value will be passed to
  [registerPrefixes()](#registerprefixes).
- The `fallback_autoloader` value will be passed to
  [setFallbackAutoloader()](#setfallbackautoloader).

### setFallbackAutoloader

```php
setFallbackAutoloader(bool $flag) : void
```

Takes a boolean flag indicating whether or not to act as a fallback autoloader
when registered with the SPL autoloader.

### isFallbackAutoloader

```php
isFallbackAutoloader() : bool
```

Indicates whether or not this instance is flagged as a fallback autoloader.

### registerNamespace

```php
registerNamespace(string $namespace, string $directory) : void
```

Register a namespace with the autoloader, pointing it to a specific directory on
the filesystem for class resolution. For classes matching that initial
namespace, the autoloader will then perform lookups within that directory.

### registerNamespaces

```php
registerNamespaces(array|Traversable $namespaces) : void
```

Register multiple namespaces with the autoloader, iterating through
`$namespaces` and passing each key and item to [registerNamespace()](#registernamespace).

### registerPrefix

```php
registerPrefix(string $prefix, string $directory) : void
```

Register a vendor prefix with the autoloader, pointing it to a specific
directory on the filesystem for class resolution. For classes matching that
initial vendor prefix, the autoloader will then perform lookups within that
directory.

### registerPrefixes

```php
registerPrefixes(array|Traversable $prefixes) : void
```

Register many vendor prefixes with the autoloader, traversing `$prefixes` and
passing each key/value pair to [registerPrefix()](#registerprefix).

### autoload

```php
autoload(string $class) : false|string
```

Attempts to load the class specified. Returns a boolean `false` on failure, or a
string indicating the class loaded on success.

### register

```php
register() : void
```

Registers the `autoload()` method of the current instance with
`spl_autoload_register()`.

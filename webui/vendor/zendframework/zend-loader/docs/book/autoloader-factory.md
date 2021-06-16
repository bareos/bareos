# The AutoloaderFactory

zend-loader provides multiple autoloader strategies.
`Zend\Loader\AutoloaderFactory` allows you to define configuration for each
strategy you wish to use and register them at once. As an example, you may have
a class map for your most used classes, but want to use a PSR-0 style autoloader
for 3rd party libraries. The factory uses configuration, allowing you to cache
your autoloader definitions or define them centrally for your application.

## Quick Start

The `AutoloaderFactory` expects an array of configuration.

```php
$config = [
    'Zend\Loader\ClassMapAutoloader' => [
        'application' => APPLICATION_PATH . '/.classmap.php',
        'zf'          => APPLICATION_PATH . '/../library/Zend/.classmap.php',
    ],
    'Zend\Loader\StandardAutoloader' => [
        'namespaces' => [
            'Phly\Mustache' => APPLICATION_PATH . '/../library/Phly/Mustache',
            'Doctrine'      => APPLICATION_PATH . '/../library/Doctrine',
        ],
    ],
];
```

Once you have your configuration in a PHP array, pass it to the
`AutoloaderFactory`:

```php
// This example assumes that the AutoloaderFactory is itself autoloadable!
use Zend\Loader\AutoloaderFactory;

AutoloaderFactory::factory($config);
```

The `AutoloaderFactory` will instantiate each autoloader with the given options,
and also call its `register()` method to register it with the SPL autoloader.

## Configuration options

The `AutoloaderFactory` expects an associative array or `Traversable` object.
Keys should be valid autoloader class names, and the values should be the
options that should be passed to the class constructor.

Internally, the `AutoloaderFactory` checks to see if the autoloader class
referenced exists. If not, it will use the [StandardAutoloader](standard-autoloader.md)
to attempt to load the class via the [include_path](http://php.net/include_path).
If the class is not found, or does not implement the
[SplAutoloader](spl-autoloader.md) interface, an exception will be raised.

## Available methods

### factory

```php
static factory(array|Traversable $options) : void
```

Instantiate and register autoloaders.

This method is **static**, and is used to instantiate autoloaders and register them
with the SPL autoloader. It expects either an array or `Traversable` object as denoted in the
[options section](#configuration options).

### getRegisteredAutoloaders

```php
static getRegisteredAutoloaders() : SplAutoloader[]
```

Retrieve a list of all autoloaders registered using the factory.

This method is **static**, and may be used to retrieve a list of all autoloaders
registered via the `factory()` method. It returns an array of `SplAutoloader`
instances.

### getRegisteredAutoloader

```php
static getRegisteredAutoloader($class) : SplAutoloader
```

Retrieve an autoloader by class name.

This method is **static**, and is used to retrieve a specific autoloader by
class name. If the autoloader is not registered, an exception will be thrown.

### unregisterAutoloaders

```php
static unregisterAutoloaders() : void
```

Unregister all autoloaders registered via the factory.

This method is **static**, and can be used to unregister all autoloaders that
were registered via the factory. Note that this will **not** unregister
autoloaders that were registered outside of the factory.

### unregisterAutoloader

```php
static unregisterAutoloader($class) : bool
```

Unregister an autoloader registered via the factory.

This method is **static**, and can be used to unregister an autoloader that was
registered via the factory. Note that this will **not** unregister autoloaders
that were registered outside of the factory. If the autoloader is registered via
the factory, after unregistering it will return `TRUE`, otherwise `FALSE`.

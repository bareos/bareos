# The ClassMapAutoloader

The `ClassMapAutoloader` is designed with performance in mind. Instead of doing
a filesystem lookup, it checks the class against an in-memory classmap, loading
the file associated with that class on a match.  This avoids unnecessary
filesystem operations, and can also ensure the autoloader "plays nice" with
opcode caches and PHP's realpath cache.

The zend-loader component provides a tool for generating classmaps via
`bin/classmap_generator.php`; read the [tool's documentation](classmap-generator.md) for more details.

## Quick Start

The first step is to generate a class map file. You may run this over any
directory containing source code anywhere underneath it.

```bash
$ php classmap_generator.php Some/Directory/
```

This will create a file named `Some/Directory/autoload_classmap.php`, which is a
PHP file returning an associative array that represents the class map.

Within your code, you will now instantiate the `ClassMapAutoloader`, and provide
it the location of the map.

```php
// This example assumes the ClassMapAutoloader is autoloadable.
use Zend\Loader\ClassMapAutoloader;

$loader = new ClassMapAutoloader();

// Register the class map:
$loader->registerAutoloadMap('Some/Directory/autoload_classmap.php');

// Register with spl_autoload:
$loader->register();
```

At this point, you may now use any classes referenced in your class map.

## Configuration Options

The `ClassMapAutoloader` expects an array of options, where each option is
either a filename referencing a class map, or an associative array of class
name/filename pairs.

As an example:

```php
// Configuration defining both a file-based class map, and an array map
$config = [
    __DIR__ . '/library/autoloader_classmap.php', // file-based class map
    [                                             // array class map
        'Application\Bootstrap' => __DIR__ . '/application/Bootstrap.php',
        'Test\Bootstrap'        => __DIR__ . '/tests/Bootstrap.php',
    ],
];
```

## Available Methods

### \_\_construct

```php
__construct(array|Traversable $options = null) : void
```

Initialize and configure the object `__construct($options = null)`; `$options`
will be passed to [setOptions()](#setoptions).

### setOptions

```php
setOptions(array|Traversable $options) : void
```

Configures the state of the autoloader, including registering class maps.
`$options` will be passed to [registerAutoloadMaps()](#registerautoloadmaps).

### registerAutoloadMap

```php
registerAutoloadMap(string|array $map) : void
```

Registers a class map with the autoloader. `$map` may be either a string
referencing a PHP script that returns a class map, or an array defining a class
map.

More than one class map may be registered; each will be merged with the
previous, meaning it's possible for a later class map to overwrite entries from
a previously registered map.

### registerAutoloadMaps

```php
registerAutoloadMaps(array|Traversable $maps) : void
```

Register multiple class maps with the autoloader, iterating over `$maps` and
passing each value to [registerAutoloadMap()](#registerautoloadmap).

### getAutoloadMap

```php
getAutoloadMap() : array
```

Retrieves the current class map as an associative array.

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

## Examples

### Using configuration to seed ClassMapAutoloader

You can use configuration to seed a `ClassMapAutoloader`; values might come from
a configuration file, a cache, or even a PHP array.  The following is an example
of a PHP array that could be used to configure the autoloader:

```php
// Configuration defining both a file-based class map, and an array map
$config = [
    APPLICATION_PATH . '/../library/autoloader_classmap.php', // file-based class map
    [                                                         // array class map
        'Application\Bootstrap' => APPLICATION_PATH . '/Bootstrap.php',
        'Test\Bootstrap'        => APPLICATION_PATH . '/../tests/Bootstrap.php',
    ],
];
```

Once you have your configuration, you can pass it either to the constructor of
the `ClassMapAutoloader`, to its `setOptions()` method, or to
`registerAutoloadMaps()`.

```php
use Zend\Loader\ClassMapAutoloader;

/* The following are all equivalent */

// To the constructor:
$loader = new ClassMapAutoloader($config);

// To setOptions():
$loader = new ClassMapAutoloader();
$loader->setOptions($config);

// To registerAutoloadMaps():
$loader = new ClassMapAutoloader();
$loader->registerAutoloadMaps($config);
```

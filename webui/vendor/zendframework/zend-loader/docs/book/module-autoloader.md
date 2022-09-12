# The ModuleAutoloader

`Zend\Loader\ModuleAutoloader` is a special implementation of the
[SplAutoloader](spl-autoloader.md) interface, and is consumed by
[zend-modulemanager](https://github.com/zendframework/zend-modulemanager) to
autoload `Module` classes from different locations.

Apart from being able to autoload modules from directories, the
`ModuleAutoloader` can also autoload modules packaged as [Phar
archives](http://php.net/phar), which allows for packaging your modules in a
single file for easier distribution. Supported archive formats are: `.phar`,
`.phar.gz`, `.phar.bz2`, `.phar.tar`, `.phar.tar.gz`, `.phar.tar.bz2`,
`.phar.zip`, `.tar`, `tar.gz`, `.tar.bz2` and `.zip`. It is, however,
recommended to avoid compressing your packages (be it either gz, bz2 or zip
compression), as it introduces additional CPU overhead to every request.

## Quickstart

As the `ModuleAutoloader` is meant to be used with the `ModuleManager`, for
examples of it's usage and how to configure it, please see the
[Module Autoloader Usage](https://docs.zendframework.com/zend-modulemanager/module-autoloader/)
documentation.

## Configuration Options

The `ModuleAutoloader` expects an array of options, where each option is either
a path to scan for modules, or a key/value pair of explicit module paths. In the
case of explicit module paths, the key is the module's name, and the value is
the path to that module.

```php
$options = [
    '/path/to/modules',
    '/path/to/other/modules',
    'MyModule' => '/explicit/path/mymodule-v1.2'
];
```

## Available Methods

### \_\_construct

```php
__construct(array|Traversable $options = null) : void
```

Initialize and configure the object; if `$options` are provided, they will be
passed to [setOptions()](#setoptions).

### setOptions

```php
setOptions(array|Traversable $options) : void
```

Configures the state of the autoloader, registering paths to modules. `$options`
will be passed to [registerPaths()](#registerpaths).

### autoload

```php
autoload(string $class) : false|string
```

Attempts to load the specified `Module` class. Returns a boolean `false` on
failure, or a string indicating the class loaded on success.

### register

```php
register() : void
```

Registers the `autoload()` method of the current instance with
`spl_autoload_register()`.

### unregister

```php
unregister() : void
```

Unregisters the `autoload()` method of the current instance with
`spl_autoload_unregister()`.

### registerPaths

```php
registerPaths(array|Traversable $paths) : void
```

Register paths to modules. For an example array, please see the
[Configuration options](#configuration-options) section.

### registerPath

```php
registerPath(string $path, string $moduleName = false) : void
```

Register a single path with the autoloader. The first parameter, `$path`, is
expected to be a string. The second parameter, `$moduleName`, is expected to be
a module name, which allows for registering an explicit path to that module.

### getPaths

```php
getPaths() : string[]
```

Returns an array of all the paths registered.

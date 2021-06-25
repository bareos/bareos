# The Module Autoloader

zend-modulemanager ships with the default module autoloader
`Zend\Loader\ModuleAutoloader`. It is a specialized autoloader responsible for
locating and on-demand loading of, the `Module` classes from a variety of
sources.

## Module Autoloader Usage

By default, the provided `Zend\ModuleManager\Listener\DefaultListenerAggregate`
sets up the `ModuleAutoloader`; as a developer, you need only provide an array
of module paths, either absolute or relative to the application's root, for the
`ModuleAutoloader` to check when loading modules. The `DefaultListenerAggregate`
will take care of instantiating and registering the `ModuleAutoloader` for you.

> ### Must be in application root
>
> In order for paths relative to your application directory to work, you must
> have the directive `chdir(dirname(__DIR__));` in your `public/index.php` file.

### Registering module paths with the `DefaultListenerAggregate`

The following example will search for modules in three different `module_paths`.
Two are local directories of this application and the third is a system-wide
shared directory.

```php
// public/index.php
use Zend\ModuleManager\Listener;
use Zend\ModuleManager\ModuleManager;

chdir(dirname(__DIR__));

// Instantiate and configure the default listener aggregate
$listenerOptions = new Listener\ListenerOptions([
    'module_paths' => [
        './module',
        './vendor',
        '/usr/share/zfmodules',
    ]
]);
$defaultListeners = new Listener\DefaultListenerAggregate($listenerOptions);

// Instantiate the module manager
$moduleManager = new ModuleManager([
    'Application',
    'FooModule',
    'BarModule',
]);

// Attach the default listener aggregate and load the modules
$moduleManager->getEventManager()->attachAggregate($defaultListeners);
$moduleManager->loadModules();
```

> ### Module paths are FIFO
>
> Module paths behave very similar to PHP's `include_path` and are searched in
> the order they are defined. If you have modules with the same name in more
> than one registered module path, the module autoloader will return the first
> one it finds.

> ### Disabling the ModuleAutoloader
>
> - Since 2.8.0
>
> If you are using Composer to autoload, you may not need to use the
> `ModuleAutoloader`. As such, you can disable it by passing the following
> option to the `ListenerOptions` class:
>
> ```php
> 'use_zend_loader' => false,
> ```
>
> If your project was begun from the [skeleton application](https://github.com/zendframework/ZendSkeletonApplication),
> place the above within the `module_listener_options` configuration of your
> `config/application.config.php` file:
>
> ```php
> return [
>     /* ... */
>     'module_listener_options' => [
>         'use_zend_loader' => false,
>         /* ... */
>     ],
>     /* ... */
> ];
> ```

## Non-Standard / Explicit Module Paths

Sometimes you may want to specify exactly where a module is instead of having
`Zend\Loader\ModuleAutoloader` try to find it in the registered paths.

### Registering a Non-Standard / Explicit Module Path

In this example, the autoloader will first check for `MyModule\Module` in
`/path/to/mymoduledir-v1.2/Module.php`. If it's not found, then it will fall
back to searching any other registered module paths.

```php
// ./public/index.php
use Zend\Loader\ModuleAutoloader;
use Zend\ModuleManager\Listener;
use Zend\ModuleManager\ModuleManager;

chdir(dirname(__DIR__));

// Instantiate and configure the default listener aggregate
$listenerOptions = new Listener\ListenerOptions([
    'module_paths' => [
        './module',
        './vendor',
        '/usr/share/zfmodules',
        'MyModule' => '/path/to/mymoduledir-v1.2',
    ]
]);
$defaultListeners = new Listener\DefaultListenerAggregate($listenerOptions);

/**
 * Without DefaultListenerAggregate:
 *
 * $moduleAutoloader = new ModuleAutoloader([
 *     './module',
 *     './vendor',
 *     '/usr/share/zfmodules',
 *     'MyModule' => '/path/to/mymoduledir-v1.2',
 * ]);
 * $moduleAutoloader->register();
 *
 */

// Instantiate the module manager
$moduleManager = new ModuleManager([
    'MyModule',
    'FooModule',
    'BarModule',
]);

// Attach the default listener aggregate and load the modules
$moduleManager->getEventManager()->attachAggregate($defaultListeners);
$moduleManager->loadModules();
```

This same method works if you provide the path to a phar archive.

## Packaging Modules with Phar

If you prefer, you may easily package your module as a
[phar archive](http://php.net/phar). The module autoloader is able to autoload
modules in the following archive formats: .phar, .phar.gz, .phar.bz2, .phar.tar,
.phar.tar.gz, .phar.tar.bz2, .phar.zip, .tar, .tar.gz, .tar.bz2, and .zip.

Package your module by performing a tar the module directory. You can then
replace the `MyModule/` directory with `MyModule.tar`, and it should still be
autoloaded without any additional changes!

> ### Avoid compression
>
> If possible, avoid using any type of compression (bz2, gz, zip) on your phar
> archives, as it introduces unnecessary CPU overhead to each request.

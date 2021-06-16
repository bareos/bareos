# Introduction to the Module System

Zend Framework 2.0 introduced a new and powerful approach to modules. This new
module system is designed with flexibility, simplicity, and re-usability in
mind. A module may contain just about anything: PHP code, including MVC
functionality; library code; view scripts; and/or public assets such as images,
CSS, and JavaScript. The possibilities are endless.

> ### Event-based system
>
> The module system in ZF2 has been designed to be a generic and powerful foundation from which
> developers and other projects can build their own module or plugin systems.
> For a better understanding of the event-driven concepts behind the ZF2 module system, it may be
> helpful to read the [EventManager documentation](https://docs.zendframework.com/zend-eventmanager/).

The module system is made up of the following:

- [The Module Autoloader](https://docs.zendframework.com/zend-loader/module-autoloader/) -
  `Zend\Loader\ModuleAutoloader` is a specialized autoloader that is responsible
  for the locating and loading of modules' `Module` classes from a variety of
  sources.
- [The Module Manager](module-manager.md) - `Zend\ModuleManager\ModuleManager`
  takes an array of module names and fires a sequence of events for each one,
  allowing the behavior of the module system to be defined entirely by the
  listeners which are attached to the module manager.
- **ModuleManager Listeners** - Event listeners can be attached to the module
  manager's various events. These listeners can do everything from resolving and
  loading modules to performing complex initialization tasks and introspection
  into each returned module object.

> ### Modules are PHP namespaces
>
> The name of a module in a Zend Framework application is a
> [PHP namespace](http://php.net/namespaces), and must follow all of the same
> rules for naming.

The recommended structure for an MVC-oriented ZF2 module is as follows:

```text
module_root/
    Module.php
    autoload_classmap.php
    autoload_function.php
    autoload_register.php
    config/
        module.config.php
    public/
        images/
        css/
        js/
    src/
        <module_namespace>/
            <code files>
    test/
        phpunit.xml
        bootstrap.php
        <module_namespace>/
            <test code files>
    view/
        <dir-named-after-module-namespace>/
            <dir-named-after-a-controller>/
                <.phtml files>
```

## The autoload\_\*.php Files

The three `autoload_*.php` files are not required, but recommended. They provide the following:

- `autoload_classmap.php` should return an array classmap of class name/filename
  pairs (with the filenames resolved via the `__DIR__` magic constant).
- `autoload_function.php` should return a PHP callback that can be passed to
  `spl_autoload_register()`. Typically, this callback should utilize the map
  returned by `autoload_classmap.php`.
- `autoload_register.php` should register a PHP callback (typically that
  returned by `autoload_function.php` with `spl_autoload_register()`.

The purpose of these three files is to provide reasonable default mechanisms for
autoloading the classes contained in the module, thus providing a trivial way to
consume the module without requiring zend-modulemanager` (e.g., for use outside
a ZF2 application).

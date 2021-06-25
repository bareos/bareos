# The PluginClassLocator interface

The `PluginClassLocator` interface describes a component capable of maintaining
an internal map of plugin names to actual class names. Classes implementing this
interface can register and unregister plugin/class associations, and return the
entire map.

## Quick Start

Classes implementing the `PluginClassLocator` must implement the following three
methods:

```php
namespace Zend\Loader;

interface PluginClassLocator
{
    public function registerPlugin($shortName, $className);
    public function unregisterPlugin($shortName);
    public function getRegisteredPlugins();
}
```

## Configuration Options

This component defines no configuration options, as it is an interface.

## Available Methods

### registerPlugin

```php
registerPlugin(string $shortName, string $className) : void
```

Implement this method to add or overwrite plugin name/class name associations in
the internal plugin map. `$shortName` will be aliased to `$className`.

### unregisterPlugin

```php
unregisterPlugin(string $shortName) : void
```

Implement this to allow removing an existing plugin mapping corresponding to
`$shortName`.

### getRegisteredPlugins

```php
getRegisteredPlugins() : array
```

Implement this to allow returning the plugin name/class name map.

# The PluginClassLoader

Resolving plugin names to class names is a common requirement within
applications.  The `PluginClassLoader` implements the interfaces
[PluginClassLocator](plugin-class-locator.md),
[ShortNameLocator](short-name-locator.md), and `IteratorAggregate`, providing a
mechanism for aliasing plugin names to classnames for later retrieval.

While it can act as a standalone class, it is intended that developers will
extend the class to provide a per-component plugin map. This allows seeding the
map with the most often-used plugins, while simultaneously allowing the end-user
to overwrite existing or register new plugins.

Additionally, `PluginClassLoader` provides the ability to statically seed all
new instances of a given `PluginClassLoader` or one of its extensions (via Late
Static Binding). If your application will always call for defining or overriding
particular plugin maps on given `PluginClassLoader` extensions, this is a
powerful capability.

## Quick Start

Typical use cases involve instantiating a `PluginClassLoader`, seeding it
with one or more plugin/class name associations, and then using it to retrieve
the class name associated with a given plugin name.

```php
use Zend\Http\HeaderLoader;

// Provide a global map, or override defaults:
HeaderLoader::addStaticMap([
    'xrequestedfor' => 'My\Http\Header\XRequestedFor',
]);

// Instantiate the loader:
$loader = new Zend\Http\HeaderLoader();

// Register a new plugin:
$loader->registerPlugin('xForwardedFor', 'My\Http\Header\XForwardedFor');

// Load/retrieve the associated plugin class:
$class = $loader->load('xrequestedfor'); // 'My\Http\Header\XRequestedFor'
```

> ### Case Sensitivity
>
> The `PluginClassLoader` is designed to do case-insensitive plugin name
> lookups. While the above example defines an "xForwardedFor" plugin name,
> internally, this will be stored as "xforwardedfor". If another plugin is
> registered with the same word but using a different casing structure, it will
> overwrite this entry.

## Configuration Options

The constructor may take a single option, an array or `Traversable` object of key/value pairs
corresponding to a plugin name and class name, respectively.

## Available Methods

### \_\_construct

```php
__construct(string|array|Traversable $map = null) : void
```

The constructor is used to instantiate and initialize the plugin class loader.
If passed a string, an array, or a `Traversable` object, it will pass this to
the [registerPlugins()](#registerplugins) method in order to seed (or overwrite)
the plugin class map.

### addStaticMap

```php
static addStaticMap(array|Traversable $map) : void
```

Static method for globally pre-seeding the loader with a class map. It accepts
either an array or `Traversable` object of plugin name/class name pairs.

When using this method, be certain you understand the precedence in which maps
will be merged; in decreasing order of preference:

- Manually registered plugin/class name pairs (e.g., via
  [registerPlugin()](#registerplugin) or
  [registerPlugins()](#registerplugins).
- A map passed to the constructor.
- The static map.
- The map defined within the class itself.

Also, please note that calling the method will **not** affect any instances
already created.

### registerPlugin

```php
registerPlugin(string $shortName, string $className) : void
```

Defined by the [PluginClassLocator](plugin-class-locator.md) interface. Expects
two string arguments, the plugin `$shortName`, and the class `$className` which
it represents.

### registerPlugins

```php
registerPlugins(string|array|Traversable $map) : void
```

If a string argument is provided, `registerPlugins()` assumes this is a class
name. If the class does not exist, an exception will be thrown. If it does, it
then instantiates the class and checks to see whether or not it implements
`Traversable`, iterating it if it does.

Each key/value pair obtained during iteration is then passed to
[registerPlugin()](#registerPlugin) using the key as the plugin name and the
value as the class name.

### unregisterPlugin

```php
unregisterPlugin(string $shortName) : void
```

Defined by the `PluginClassLocator` interface; remove a plugin/class association
from the plugin class map.

### getRegisteredPlugins

```php
getRegisteredPlugins() : array
```

Defined by the `PluginClassLocator` interface; return the entire plugin class
map as an array.

### isLoaded

```php
isLoaded(string $name) : bool
```

Defined by the `ShortNameLocator` interface; determine if the given plugin has
been resolved to a class name.

### getClassName

```php
getClassName(string $name) : string
```

Defined by the `ShortNameLocator` interface; return the class name to which a
plugin name resolves.

### load

```php
load(string $name) : string|false
```

Defined by the `ShortNameLocator` interface; attempt to resolve a plugin name to
a class name. If successful, returns the class name; otherwise, returns a
boolean `false`.

### getIterator

```php
getIterator() : Traversable
```

Defined by the `IteratorAggregate` interface; allows iteration over the plugin
class map. This can come in useful for using `PluginClassLoader` instances to
other `PluginClassLoader` instances in order to merge maps.

## Examples

### Using Static Maps

It's often convenient to provide global overrides or additions to the maps in a
`PluginClassLoader` instance. This can be done using the `addStaticMap()`
method:

```php
use Zend\Loader\PluginClassLoader;

PluginClassLoader::addStaticMap([
    'xrequestedfor' => 'My\Http\Header\XRequestedFor',
]);
```

Any later instances created will now have this map defined, allowing you to load
that plugin.

```php
use Zend\Loader\PluginClassLoader;

$loader = new PluginClassLoader();
$class = $loader->load('xrequestedfor'); // My\Http\Header\XRequestedFor
```

### Creating a pre-loaded map

In many cases, you know exactly which plugins you may be drawing upon on a
regular basis, and which classes they will refer to. In this case, extend
the `PluginClassLoader` and define the map within the extending class.

```php
namespace My\Plugins;

use Zend\Loader\PluginClassLoader;

class PluginLoader extends PluginClassLoader
{
    /**
     * @var array Plugin map
     */
    protected $plugins = [
        'foo'    => 'My\Plugins\Foo',
        'bar'    => 'My\Plugins\Bar',
        'foobar' => 'My\Plugins\FooBar',
    ];
}
```

At this point, you can instantiate the map and immediately use it.

```php
$loader = new My\Plugins\PluginLoader();
$class  = $loader->load('foobar'); // My\Plugins\FooBar
```

`PluginClassLoader` makes use of late static binding, allowing per-class static
maps. If you want to allow defining a [static map](#using-static-maps) specific
to this extending class, declare a protected static `$staticMap` property:

```php
namespace My\Plugins;

use Zend\Loader\PluginClassLoader;

class PluginLoader extends PluginClassLoader
{
    protected static $staticMap = [];

    // ...
}
```

To inject the static map, call the `addStaticMap()` on the extension class:

```php
PluginLoader::addStaticMap([
    'baz' => 'My\Plugins\Baz',
]);
```

### Extending a plugin map using another plugin map

In some cases, a general map class may already exist; as an example, several
Zend Framework components defining plugin brokers have an associated
`PluginClassLoader` extension defining the plugins available for that component
within the framework. What if you want to define some additions to these? Where
should that code go?

One possibility is to define the map in a configuration file, and then inject
the configuration into an instance of the plugin loader.

Another solution is to define a new plugin map class. The class name or an
instance of the class may then be passed to the constructor or
`registerPlugins()`.

```php
namespace My\Plugins;

use Zend\Loader\PluginClassLoader;
use Zend\Http\HeaderLoader;

class PluginLoader extends PluginClassLoader
{
    /**
     * @var array Plugin map
     */
    protected $plugins = [
        'foo'    => 'My\Plugins\Foo',
        'bar'    => 'My\Plugins\Bar',
        'foobar' => 'My\Plugins\FooBar',
    ];
}

// Inject in constructor:
$loader = new HeaderLoader(PluginLoader::class); // as string class name
$loader = new HeaderLoader(new PluginLoader());  // as instance

// Or via registerPlugins():
$loader->registerPlugins(PluginLoader::class); // as string class name
$loader->registerPlugins(new PluginLoader());  // as instance
```

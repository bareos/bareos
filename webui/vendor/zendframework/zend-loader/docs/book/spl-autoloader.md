# The SplAutoloader Interface

While any valid PHP callback may be registered with `spl_autoload_register()`,
the autoloaders zend-loader provides offer more flexibility by being stateful
and allowing configuration. To provide a common interface for such autoloaders,
zend-loader provides the `SplAutoloader` interface.

Objects implementing this interface provide a standard mechanism for
configuration, a method that may be invoked to attempt to load a class, and a
method for registering with the SPL autoloading mechanism.

## Quick Start

To create your own autoloading mechanism, create a class implementing the
`SplAutoloader` interface (you may review the methods defined in the [methods
section](#available-methods)). As an example, consider the following autoloader,
which will look for a class file named after the class within a list of
registered directories.

```php
namespace Custom;

use InvalidArgumentException;
use Traversable;
use Zend\Loader\SplAutoloader;

class ModifiedIncludePathAutoloader implements SplAutoloader
{
    protected $paths = array();

    public function __construct($options = null)
    {
        if (null !== $options) {
            $this->setOptions($options);
        }
    }

    public function setOptions($options)
    {
        if (! is_array($options) && ! $options instanceof Traversable) {
            throw new InvalidArgumentException();
        }

        foreach ($options as $path) {
            if (! in_array($path, $this->paths)) {
                $this->paths[] = $path;
            }
        }
    }

    public function autoload($classname)
    {
        $filename = $classname . '.php';
        foreach ($this->paths as $path) {
            $test = sprintf('%s/%s', $path, $filename);
            if (file_exists($test)) {
                return include($test);
            }
        }
        return false;
    }

    public function register()
    {
        spl_autoload_register([$this, 'autoload']);
    }
}
```

To use this `ModifiedIncludePathAutoloader` from the previous example:

```php
$options = [
   '/path/one',
   '/path/two',
];
$autoloader = new Custom\ModifiedIncludePathAutoloader($options);
$autoloader->register();
```

## Configuration Options

This component defines no configuration options, as it is an interface.

## Available Methods

### \_\_construct

```php
__construct($options = null) : void
```

Autoloader constructors should optionally receive configuration. Typically, if
received, these will be passed to the `setOptions()` method to process.

### setOptions

```php
setOptions(array|Traversable $options) : void
```

Used to configure the autoloader. Typically, it should expect either an array or
a `Traversable` object, though validation of the options is left to
implementation.

### autoload

```php
autoload(string $class) : false|string
```

This method should be used to resolve a class name to the file defining it. When
a positive match is found, return the class name; otherwise, return a boolean
`false`.

### register

```php
register() : void
```

Should be used to register the autoloader instance with
`spl_autoload_register()`. Invariably, the method should look like the
following:

```php
public function register()
{
    spl_autoload_register([$this, 'autoload']);
}
```

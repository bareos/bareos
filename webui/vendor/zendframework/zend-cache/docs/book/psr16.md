# PSR-16

> Available since version 2.8.0

[PSR-16](https://www.php-fig.org/psr/psr-16/) provides a simplified approach to
cache access that does not involve cache pools, tags, deferment, etc.; it
can be thought of as a key/value storage approach to caching.

zend-cache provides PSR-16 support via the class
`Zend\Cache\Psr\SimpleCache\SimpleCacheDecorator`. This class implements PSR-16's
`Psr\SimpleCache\CacheInterface`, and composes a
`Zend\Cache\Storage\StorageInterface` instance to which it proxies all
operations.

Instantiation is as follows:

```php
use Zend\Cache\StorageFactory;
use Zend\Cache\Psr\SimpleCache\SimpleCacheDecorator;

$storage = StorageFactory::factory([
    'adapter' => [
        'name'    => 'apc',
        'options' => [],
    ],
]);

$cache = new SimpleCacheDecorator($storage);
```

Once you have a `SimpleCacheDecorator` instance, you can perform operations per
that specification:

```php
// Use has() to determine whether to fetch the value or calculate it:
$value = $cache->has('someKey') ? $cache->get('someKey') : calculateValue();
if (! $cache->has('someKey')) {
    $cache->set('someKey', $value);
}

// Or use a default value:
$value = $cache->get('someKey', $defaultValue);
```

When setting values, whether single values or multiple, you can also optionally
provide a Time To Live (TTL) value. This proxies to the underlying storage
instance's options, temporarily resetting its TTL value for the duration of the
operation. TTL values may be expressed as integers (in which case they represent
seconds) or `DateInterval` instances. As examples:

```php
$cache->set('someKey', $value, 30); // set TTL to 30s
$cache->set('someKey', $value, new DateInterval('P1D'); // set TTL to 1 day

$cache->setMultiple([
    'key1' => $value1,
    'key2' => $value2,
], 3600); // set TTL to 1 hour
$cache->setMultiple([
    'key1' => $value1,
    'key2' => $value2,
], new DateInterval('P6H'); // set TTL to 6 hours
```

For more details on what methods are exposed, consult the [CacheInterface
specification](https://www.php-fig.org/psr/psr-16/#21-cacheinterface).

## Serialization

PSR-16 has strict requirements around serialization of values. This is done to
ensure that if you swap one PSR-16 adapter for another, the new one will be able
to return the same values that the original adapter saved to the cache.

Not all cache backends support the same data types, however. zend-cache provides
a plugin, `Zend\Cache\Storage\Plugin\Serializer`, that you can attach to
adapters in order to ensure data is serialized to a string when saving to the
cache, and deserialized to native PHP types on retrieval. The following adapters
require this plugin in order to work with the `SimpleCacheDecorator`:

- Dba
- Filesystem
- Memcache
- MongoDB
- Redis
- XCache

We provide a number of examples of [attaching plugins to storage adapters in the
plugins chapter](storage/plugin.md). Generally, it will be one of:

```php
// Manual attachment after you have an instance:
$cache->addPlugin(new Serializer());

// Via configuration:
$cache = StorageFactory::factory([
    'adapter' => 'filesystem',
    'plugins' => [
        'serializer',
    ],
]);
```

## Deleting Items and Exceptions

PSR-16 states that the `delete()` and `deleteMultiple()` methods should return
`false` if an _error_ occured when deleting the key(s) provided, but `true`
otherwise.

Generally, zend-cache storage adapters comply with this. However, it is possible
to configure your adapter such that you may get a false positive result from
these methods.

When an exception is raised and caught during key removal by an adapter, the
adapter triggers an event with a `Zend\Cache\Storage\ExceptionEvent`. Plugins 
can react to these, and even manipulate the event instance. One such plugin,
`Zend\Cache\Storage\Plugin\ExceptionHandler`, has a configuration option,
`throw_exceptions` that, when boolean `false`, will prevent raising the
exception. In such cases, adapters will typically return a boolean `false`
anyways, but custom, third-party adapters may not.

Additionally, if you add a custom plugin that listens to removal event
exceptions and modifies the return value and/or disables throwing the exception,
a false positive return value could occur.

As such, we recommend that if you wish to use zend-cache to provide a PSR-16
adapter, you audit the plugins you use with your adapter to ensure that you will
get consistent, correct behavior for `delete()` and `deleteMultiple()`
operations.

# PSR-6

> Available since version 2.8.0

## Overview

The `Zend\Cache\Psr\CacheItemPool\CacheItemPoolDecorator` provides a [PSR-6](https://www.php-fig.org/psr/psr-6/)
compliant wrapper for supported storage adapters.

PSR-6 specifies a common interface to cache storage, enabling developers to switch between implementations without
having to worry about any behind-the-scenes differences between them.


## Quick Start

To use the pool, instantiate your storage as normal, then pass it to the
`CacheItemPoolDecorator`.

```php
use Zend\Cache\StorageFactory;
use Zend\Cache\Psr\CacheItemPool\CacheItemPoolDecorator;

$storage = StorageFactory::factory([
    'adapter' => [
        'name'    => 'apc',
        'options' => [],
    ],
]);

$pool = new CacheItemPoolDecorator($storage);

// attempt to get an item from cache
$item = $pool->getItem('foo');

// check whether item was found
if (! $item->isHit()) {
    // ...
    // perform expensive operation to calculate $value for 'foo'
    // ...

    $item->set($value);
    $pool->save($item);
}

// use the value of the item
echo $item->get();
```

Note that you will always get back a `CacheItem` object, whether it was found in cache or not: this is so `false`-y
values like an empty string, `null`, or `false` can be stored. Always check `isHit()` to determine if the item was
found.


## Supported Adapters

The PSR-6 specification requires that the underlying storage support time-to-live (TTL), which is set when the
item is saved. For this reason the following adapters cannot be used: `Dba`, `Filesystem`, `Memory` and `Session`. The
`XCache` adapter calculates TTLs based on the request time, not the time the item is actually persisted, which means
that it also cannot be used.

In addition adapters must support the `Zend\Cache\FlushableInterface`. All the current `Zend\Cache\Storage\Adapter`s
fulfil this requirement.

Attempting to use an unsupported adapter will throw an exception implementing `Psr\Cache\CacheException`.

The `Zend\Cache\Psr\CacheItemPool\CacheItemPoolDecorator` adapter doesn't support driver deferred saves, so cache items are saved
on destruct or on explicit `commit()` call.

### Quirks

#### APC

You cannot set the [`apc.use_request_time`](http://php.net/manual/en/apc.configuration.php#ini.apc.use-request-time)
ini setting with the APC adapter: the specification requires that all TTL values are calculated from when the item is
actually saved to storage. If this is set when you instantiate the pool it will throw an exception implementing
`Psr\Cache\CacheException`. Changing the setting after you have instantiated the pool will result in non-standard
behaviour.


## Logging Errors

The specification [states](https://github.com/php-fig/fig-standards/blob/master/accepted/PSR-6-cache.md#error-handling):

> While caching is often an important part of application performance, it should never be a critical part of application
> functionality. Thus, an error in a cache system SHOULD NOT result in application failure.

Once you've got your pool instance, almost all exceptions thrown by the storage will be caught and ignored. The only
storage exceptions that bubble up implement `Psr\Cache\InvalidArgumentException` and are typically caused by invalid
key errors. To be PSR-6 compliant, cache keys must not contain the following characters: `{}()/\@:`. However different
storage adapters may have further restrictions. Check the documentation for your particular adapter to be sure.

We strongly recommend tracking exceptions caught from storage, either by logging them or recording them in some other
way. Doing so is as simple as adding an [`ExceptionHandler` plugin](zend.cache.storage.plugin.html#3.4). Say you have a
[PSR-3](https://github.com/php-fig/fig-standards/blob/master/accepted/PSR-3-logger-interface.md) compliant logger
called `$logger`:


```php
$cacheLogger = function (\Exception $e) use ($logger) {
    $message = sprintf(
        '[CACHE] %s:%s %s "%s"',
        $exception->getFile(),
        $exception->getLine(),
        $exception->getCode(),
        $exception->getMessage()
    );
    $logger->error($message);
};
                                }
$storage = StorageFactory::factory([
    'adapter' => [
        'name'    => 'apc',
    ],
    'plugins' => [
        'exceptionhandler' => [
            'exception_callback' => $cacheLogger,
            'throw_exceptions' => true,
        ],
    ],
]);

$pool = new CacheItemPoolDecorator($storage);
```

Note that `throw_exceptions` should always be `true` (the default) or you will not get the correct return values from
calls on the pool such as `save()`.


## Supported Data Types

As per [the specification](https://github.com/php-fig/fig-standards/blob/master/accepted/PSR-6-cache.md#data), the
following data types can be stored in cache: `string`, `integer`, `float`, `boolean`, `null`, `array`, `object` and be
returned as a value with exactly the same type.

Not all adapters can natively store all these types. For instance, Redis stores booleans and integers as a string. Where
this is the case *all* values will be automatically run through `serialize()` on save and `unserialize()` on get: you
do not need to use a `Zend\Cache\Storage\Plugin\Serializer` plugin.


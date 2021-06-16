# Session Save Handlers

zend-session comes with a set of save handler classes.  Save handlers themselves
are decoupled from PHP's save handler functions and are only implemented as a
PHP save handler when utilized in conjunction with
`Zend\Session\SessionManager`.

## Cache

`Zend\Session\SaveHandler\Cache` allows you to provide an instance of
`Zend\Cache\Storage\Adapter\AdapterInterface` to be utilized as a session save
handler. Generally if you are utilizing the `Cache` save handler; you are likely
using products such as memcached.

### Basic usage

A basic example is one like the following:

```php
use Zend\Cache\StorageFactory;
use Zend\Session\SaveHandler\Cache;
use Zend\Session\SessionManager;

$cache = StorageFactory::factory([
    'adapter' => [
       'name' => 'memcached',
       'options' => [
           'server' => '127.0.0.1',
       ],
    ],
]);

$saveHandler = new Cache($cache);
$manager = new SessionManager();
$manager->setSaveHandler($saveHandler);
```

## DbTableGateway

`Zend\Session\SaveHandler\DbTableGateway` allows you to utilize
`Zend\Db\TableGateway\TableGatewayInterface` implementations as a session save
handler. Setup of a `DbTableGateway` save handler requires an instance of
`Zend\Db\TableGateway\TableGatewayInterface` and an instance of
`Zend\Session\SaveHandler\DbTableGatewayOptions`. In the most basic setup, a
`TableGateway` object and using the defaults of the `DbTableGatewayOptions` will
provide you with what you need.

### Creating the database table

```sql
CREATE TABLE `session` (
    `id` char(32),
    `name` char(32),
    `modified` int,
    `lifetime` int,
    `data` text,
     PRIMARY KEY (`id`, `name`)
);
```

### Basic usage

```php
use Zend\Db\TableGateway\TableGateway;
use Zend\Session\SaveHandler\DbTableGateway;
use Zend\Session\SaveHandler\DbTableGatewayOptions;
use Zend\Session\SessionManager;

$tableGateway = new TableGateway('session', $adapter);
$saveHandler  = new DbTableGateway($tableGateway, new DbTableGatewayOptions());
$manager      = new SessionManager();
$manager->setSaveHandler($saveHandler);
```

## MongoDB

`Zend\Session\SaveHandler\MongoDB` allows you to provide a MongoDB collection to
be utilized as a session save handler. You provide the options in the
`Zend\Session\SaveHandler\MongoDBOptions` class. You must install the
[mongodb PHP extensions](http://php.net/mongodb) and the
[MongoDB PHP library](https://github.com/mongodb/mongo-php-library).

### Basic Usage

```php
use MongoDB\Client;
use Zend\Session\SaveHandler\MongoDB;
use Zend\Session\SaveHandler\MongoDBOptions;
use Zend\Session\SessionManager;

$mongoClient = new Client();
$options = new MongoDBOptions([
    'database'   => 'myapp',
    'collection' => 'sessions',
]);
$saveHandler = new MongoDB($mongoClient, $options);
$manager     = new SessionManager();
$manager->setSaveHandler($saveHandler);
```

## Custom Save Handlers

There may be cases where you want to create a save handler.  Creating a custom
save handler is much like creating a custom PHP save handler, with minor
differences. All zend-session-compatible save handlers *must* implement
`Zend\Session\SaveHandler\SaveHandlerInterface`.  Additionally, if your save
handler has configurable functionality, you will also need to create an options
class.

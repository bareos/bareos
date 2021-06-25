# Session Storage

zend-session comes with a standard set of storage handlers. Storage handlers are
the intermediary between when the session starts and when the session writes and
closes.  The default session storage is
`Zend\Session\Storage\SessionArrayStorage`.

## Array Storage

`Zend\Session\Storage\ArrayStorage` provides a facility to store all information
in an `ArrayObject`. This storage method is likely incompatible with 3rd party
libraries and all properties will be inaccessible through the `$_SESSION`
superglobal. Additionally `ArrayStorage` will not automatically repopulate the
storage container in the case of each new request and would have to manually be
re-populated.

### Basic Usage

```php
use Zend\Session\Storage\ArrayStorage;
use Zend\Session\SessionManager;

$populateStorage = ['foo' => 'bar'];
$storage         = new ArrayStorage($populateStorage);
$manager         = new SessionManager();
$manager->setStorage($storage);
```

## Session Storage

`Zend\Session\Storage\SessionStorage` replaces `$_SESSION,` providing a facility
to store all information in an `ArrayObject`. This means that it may not be
compatible with 3rd party libraries, although information stored in the
`$_SESSION` superglobal should be available in other scopes.

### Basic Usage

```php
use Zend\Session\Storage\SessionStorage;
use Zend\Session\SessionManager;

$manager = new SessionManager();
$manager->setStorage(new SessionStorage());
```

## Session Array Storage

`Zend\Session\Storage\SessionArrayStorage` provides a facility to store all
information directly in the `$_SESSION` superglobal. This storage class provides
the most compatibility with 3rd party libraries and allows for directly storing
information into `$_SESSION`.

### Basic Usage

```php
use Zend\Session\Storage\SessionArrayStorage;
use Zend\Session\SessionManager;

$manager = new SessionManager();
$manager->setStorage(new SessionArrayStorage());
```

## Custom Storage

To create a custom storage container, you *must* implement
`Zend\Session\Storage\StorageInterface`. This interface extends each of
`ArrayAccess`, `Traversable`, `Serializable`, and `Countable`, and it is in the
methods those define that the majority of implementation occurs. The following
methods must also be implemented:

```php
public function getRequestAccessTime() : int;

public function lock(int|string $key = null) : void;
public function isLocked(int|string $key = null) : bool;
public function unlock(int|string $key = null) : void;

public function markImmutable() : void;
public function isImmutable() : bool;

public function setMetadata(string $key, mixed $value, bool $overwriteArray = false) : void;
public function getMetadata(string $key = null) : mixed;

public function clear(inst|string $key = null) : void;

public function fromArray(array $array) : void;
public function toArray(bool $metaData = false) : array;
```

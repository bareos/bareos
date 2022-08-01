# Session Validators

Session validators provide protections against session hijacking.

## Http User Agent

`Zend\Session\Validator\HttpUserAgent` provides a validator to check the session
against the originally stored `$_SERVER['HTTP_USER_AGENT']` variable. Validation
will fail in the event that this does not match and throws an exception in
`Zend\Session\SessionManager` after `session_start()` has been called.

### Basic Usage

```php
use Zend\Session\Validator\HttpUserAgent;
use Zend\Session\SessionManager;

$manager = new SessionManager();
$manager->getValidatorChain()
    ->attach('session.validate', [new HttpUserAgent(), 'isValid']);
```

## Remote Addr

`Zend\Session\Validator\RemoteAddr` provides a validator to check the session
against the originally stored `$_SERVER['REMOTE_ADDR']` variable. Validation
will fail in the event that this does not match and throws an exception in
`Zend\Session\SessionManager` after `session_start()` has been called.

### Basic Usage

```php
use Zend\Session\Validator\RemoteAddr;
use Zend\Session\SessionManager;

$manager = new SessionManager();
$manager->getValidatorChain()
    ->attach('session.validate', [new RemoteAddr(), 'isValid']);
```

## Custom Validators

You may want to provide your own custom validators to validate against other
items from storing a token and validating a token to other various techniques.
To create a custom validator you *must* implement the validation interface
`Zend\Session\Validator\ValidatorInterface`.

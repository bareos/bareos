# HTTP Client

`Zend\Http\Client` provides an interface for performing Hyper-Text Transfer
Protocol (HTTP) requests. `Zend\Http\Client` supports all basic features
expected from an HTTP client, as well as some more complex features such as HTTP
authentication and file uploads. Successful requests (and most unsuccessful ones
too) return a `Zend\Http\Response` object, which provides access to the
response's headers and body (see the chapter on [Responses](../response.md) for
more details).

## Quick Start

The class constructor optionally accepts a URL as its first parameter (which can
be either a string or a `Zend\Uri\Http` object), and an array or `Traversable`
object containing configuration options. The `send()` method is used to submit
the request to the remote server, and a `Zend\Http\Response` object is returned:

```php
use Zend\Http\Client;

$client = new Client(
    'http://example.org',
    [
        'maxredirects' => 0,
        'timeout'      => 30,
    ]
);
$response = $client->send();
```

Both constructor parameters can be left out, and set later using the `setUri()`
and `setOptions()` methods:

```php
use Zend\Http\Client;

$client = new Client();
$client->setUri('http://example.org');
$client->setOptions([
    'maxredirects' => 0,
    'timeout'      => 30,
]);
$response = $client->send();
```

`Zend\Http\Client` can also dispatch requests using a separately configured
`request` object (see the [Request](../request.md) manual for full details of
the methods available):

```php
use Zend\Http\Client;
use Zend\Http\Request;

$request = new Request();
$request->setUri('http://example.org');

$client = new Client();

$response = $client->send($request);
```

> ### URL validation
>
> `Zend\Http\Client` uses `Zend\Uri\Http` to validate URLs. See the
> [zend-uri](http://framework.zend.com/manual/current/en/index.html#zend-uri)
> documentation for more information.

## Configuration

The constructor and `setOptions()` method accept an associative array or
`Traversable` instance containing configuration parameters. Setting these
parameters is optional, as they all have default values.

Parameter         | Description                                                                                                                                                                          | Expected Values | Default Value
------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------|--------------
`maxredirects`    | Maximum number of redirections to follow (0 = none)                                                                                                                                  | integer         | 5
`strictredirects` | Whether to strictly follow the RFC when redirecting (see this section)                                                                                                               | boolean         | FALSE
`useragent`       | User agent identifier string (sent in request headers)                                                                                                                               | string          | `Zend\Http\Client`
`timeout`         | Connection timeout (seconds)                                                                                                                                                         | integer         | 10
`httpversion`     | HTTP protocol version (usually '1.1' or '1.0')                                                                                                                                       | string          | 1.1
`adapter`         | Connection adapter class to use (see this section)                                                                                                                                   | mixed           | `Zend\Http\Client\Adapter\Socket`
`keepalive`       | Whether to enable keep-alive connections with the server. Useful and might improve performance if several consecutive requests to the same server are performed.                     | boolean         | FALSE
`storeresponse`   | Whether to store last response for later retrieval with getLastResponse(). If set to FALSE, getLastResponse() will return NULL.                                                      | boolean         | TRUE
`encodecookies`   | Whether to pass the cookie value through urlencode/urldecode. Enabling this breaks support with some web servers. Disabling this limits the range of values the cookies can contain. | boolean         | TRUE
`outputstream`    | Destination for streaming of received data (options: string (filename), true for temp file, false/null to disable streaming)                                                         | boolean         | FALSE
`rfc3986strict`   | Whether to strictly adhere to RFC 3986 (in practice, this means replacing '+' with '%20')                                                                                            | boolean         | FALSE
`sslcapath`       | Path to SSL certificate directory                                             | string          | `NULL`
`sslcafile`       | Path to Certificate Authority (CA) bundle                                     | string          | `NULL`

The options are also passed to the adapter class upon instantiation, so the same
configuration can be used for adapter configuration. See the
[adapters](adapters.md) section for more information on the adapter-specific
options available.

## Examples

### Performing a GET request

GET is the default method used, and requires no special configuration.

```php
use Zend\Http\Client;

$client = new Client('http://example.org');
$response = $client->send();
```

### Using request methods other than GET

The request method can be set using `setMethod()`. If no method is specified,
the method set by the last `setMethod()` call is used. If `setMethod()` was
never called, the default request method is `GET`.

```php
use Zend\Http\Client;

$client = new Client('http://example.org');

// Performing a POST request
$client->setMethod('POST');
$response = $client->send();
```

For convenience, `Zend\Http\Request` defines all request methods as class
constants: `Zend\Http\Request::METHOD_GET`, `Zend\Http\Request::METHOD_POST` and
so on.

```php
use Zend\Http\Client;
use Zend\Http\Request;

$client = new Client('http://example.org');

// Performing a POST request
$client->setMethod(Request::METHOD_POST);
$response = $client->send();
```

### Setting query parameters

Adding query parameters to an HTTP request can be done either by specifying them
as part of the URL, or by using the `setParameterGet()` method. This method
takes the query parameters as an associative array of name/value pairs.

```php
use Zend\Http\Client;
$client = new Client();

// This is equivalent to setting a URL in the Client's constructor:
$client->setUri('http://example.com/index.php?knight=lancelot');

// Adding several parameters with one call
$client->setParameterGet([
    'first_name'  => 'Bender',
    'middle_name' => 'Bending',
    'last_name'   => 'Rodríguez',
    'made_in'     => 'Mexico',
]);
```

### Setting form-encoded body parameters

While query parameters can be sent with every request method, other methods can
accept parameters via the request body. In many cases, these are
`application/x-www-form-urlencoded` parameters; zend-http allows you to specify
such parameters usingthe `setParameterPost()` method, which is identical to the
`setParameterGet()` method in structure.

```php
use Zend\Http\Client;

$client = new Client();

// Setting several POST parameters, one of them with several values
$client->setParameterPost([
    'language'  => 'es',
    'country'   => 'ar',
    'selection' => [45, 32, 80],
]);
```

Note that when sending `POST` requests (or an request allowing a request body),
you can set both query and `POST` parameters. On the other hand, setting POST
parameters on a `GET` request will not trigger an error, rendering it useless.

### Connecting to SSL URLs

If you are trying to connect to an SSL or TLS (https) URL and are using the
default (`Zend\Http\Client\Adapter\Socket`) adapter, you may need to set the
`sslcapath` configuration option in order to allow PHP to validate the SSL
certificate:

```php
use Zend\Http\Client;

$client = new Client(
    'https://example.org',
    [
        'sslcapath' => '/etc/ssl/certs',
    ]
);
$response = $client->send();
```

The exact path to use will vary depending on your operating system. Without this
you'll get the exception "Unable to enable crypto on TCP connection" when trying
to connect.

Alternatively, you could switch to the curl adapter, which negotiates SSL
connections more transparently:

```php
use Zend\Http\Client;

$client = new Client(
    'https://example.org',
    [
        'adapter' => 'Zend\Http\Client\Adapter\Curl',
    ]
);
$response = $client->send();
```

## Complete Example

```php
use Zend\Http\Client;

$client = new Client();
$client->setUri('http://www.example.com');
$client->setMethod('POST');
$client->setParameterPost([
    'foo' => 'bar',
]);

$response = $client->send();

if ($response->isSuccess()) {
    // the POST was successful
}
```

or the same thing, using a request object:

```php
use Zend\Http\Client;
use Zend\Http\Request;

$request = new Request();
$request->setUri('http://www.example.com');
$request->setMethod('POST');
$request->getPost()->set('foo', 'bar');

$client = new Client();
$response = $client->send($request);

if ($response->isSuccess()) {
    // the POST was successful
}
```

# HTTP Client Connection Adapters

`Zend\Http\Client` is based on a connection adapter design. The connection adapter is the object in
charge of performing the actual connection to the server, as well as writing requests and reading
responses. This connection adapter can be replaced, and you can create and extend the default
connection adapters to suit your special needs, without the need to extend or replace the entire
HTTP client class.

Currently, zend-http provides four built-in client connection adapters:

- `Zend\Http\Client\Adapter\Socket` (default)
- `Zend\Http\Client\Adapter\Proxy`
- `Zend\Http\Client\Adapter\Curl`
- `Zend\Http\Client\Adapter\Test`

The client can accept an adapter via either its `setAdapter()` method, or during
instantiation via its `adapter` configuration option. When using configuration,
the value of the `adapter` option may be one of:

- an adapter instance
- the fully qualified class name of an adapter

## The Socket adapter

The default connection adapter used when none is specified is the
`Zend\Http\Client\Adapter\Socket` adapter.  The `Socket` adapter is based on
PHP's built-in `fsockopen()` function, and does not require any special
extensions or compilation flags.

The `Socket` adapter allows several extra configuration options that can be set
via either the client's constructor or `setOptions()` method:

Parameter            | Description                                                                          | Expected Type | Default Value
---------------------|--------------------------------------------------------------------------------------|---------------|--------------
`persistent`         | Whether to use persistent TCP connections                                            | boolean       | `FALSE`
`ssltransport`       | SSL transport layer (eg. `sslv2`, `tls`)                                             | string        | `ssl`
`sslcert`            | Path to a PEM encoded SSL certificate                                                | string        | `NULL`
`sslpassphrase`      | Passphrase for the SSL certificate file                                              | string        | `NULL`
`sslverifypeer`      | Whether to verify the SSL peer                                                       | string        | `TRUE`
`sslcapath`          | Path to SSL certificate directory                                                    | string        | `NULL`
`sslallowselfsigned` | Whether to allow self-signed certificates                                            | string        | `FALSE`
`sslusecontext`      | Enables proxied connections to use SSL even if the proxy connection itself does not. | boolean       | `FALSE`

> ### Persistent TCP connections
>
> Using persistent TCP connections can potentially speed up HTTP requests, but
> in most use cases, will have little positive effect and might overload the
> HTTP server you are connecting to. It is recommended to use persistent TCP
> connections only if you connect to the same server very frequently, and are
> sure that the server is capable of handling a large number of concurrent
> connections. In any case you are encouraged to benchmark the effect of
> persistent connections on both the client speed and server load before using
> this option.
>
> Additionally, when using persistent connections, we recommend enabling
> Keep-Alive HTTP requests as described in [the client configuration section](intro.md#configuration);
> otherwise persistent connections might have little or no effect.

> ### HTTPS SSL stream parameters
>
> `ssltransport`, `sslcert` and `sslpassphrase` are only relevant when
> connecting using HTTPS.  While the default SSL/TLS settings should work for
> most applications, you might need to change them if the server you are
> connecting to requires special client setup. If so, please read the
> [PHP manual chapter on SSL and TLS transport options](http://php.net/transports.inet).

### Changing the HTTPS transport layer

```php
use Zend\Http\Client;

// Set the configuration parameters
$config = [
    'adapter'      => Client\Adapter\Socket::class,
    'ssltransport' => 'tls',
];

// Instantiate a client object
$client = new Client('https://www.example.com', $config);

// The following request will be sent over a TLS secure connection.
$response = $client->send();
```

The result of the example above will be similar to opening a TCP connection
using the following PHP command:

```php
fsockopen('tls://www.example.com', 443);
```

### Customizing and accessing the Socket adapter stream context

`Zend\Http\Client\Adapter\Socket` provides direct access to the underlying
[stream context](http://php.net/stream.contexts) used to connect to the remote
server. This allows the user to pass specific options and parameters to the TCP
stream, and to the SSL wrapper in case of HTTPS connections.

You can access the stream context using the following methods of
`Zend\Http\Client\Adapter\Socket`:

- `setStreamContext($context)`: Sets the stream context to be used by the
  adapter. Can accept either a stream context resource created using the
  [stream_context_create()](http://php.net/stream_context_create) PHP function,
  or an array of stream context options, in the same format provided to this
  function.  Providing an array will create a new stream context using these
  options, and set it.
- `getStreamContext()`: Get the current stream context of the adapter. If no
  stream context was set, this method will create a default stream context and
  return it. You can then set or get the value of different context options
  using regular PHP stream context functions.

#### Setting stream context options for the Socket adapter

```php
use Zend\Http\Client;

// Array of options
$options = [
    'socket' => [
        // Bind local socket side to a specific interface
        'bindto' => '10.1.2.3:50505',
    ],
    'ssl'    => [
        // Verify server side certificate,
        // do not accept invalid or self-signed SSL certificates
        'verify_peer'       => true,
        'allow_self_signed' => false,

        // Capture the peer's certificate
        'capture_peer_cert' => true,
    ],
];

// Create an adapter object and attach it to the HTTP client:
$adapter = new Client\Adapter\Socket();
$client  = new Client();
$client->setAdapter($adapter);

// Method 1: pass the options array to setStreamContext():
$adapter->setStreamContext($options);

// Method 2: create a stream context and pass it to setStreamContext():
$context = stream_context_create($options);
$adapter->setStreamContext($context);

// Method 3: get the default stream context and set the options on it:
$context = $adapter->getStreamContext();
stream_context_set_option($context, $options);

// Now, perform the request:
$response = $client->send();

// If everything went well, you can now access the context again:
$opts = stream_context_get_options($adapter->getStreamContext());
echo $opts['ssl']['peer_certificate'];
```

> #### Set stream context options prior to requests
>
> Note that you must set any stream context options before using the adapter to
> perform actual requests. If no context is set before performing HTTP requests
> with the `Socket` adapter, a default stream context will be created. This
> context resource could be accessed after performing any requests using the
> `getStreamContext()` method.

## The Proxy adapter

`Zend\Http\Client\Adapter\Proxy` is similar to the default `Socket` adapter; the
primary difference is that the connection is made through an HTTP proxy server
instead of a direct connection to the target server. This allows usage of
`Zend\Http\Client` behind proxy servers, which is sometimes required for
security or performance reasons.

Using the `Proxy` adapter requires several additional client configuration
parameters to be set, in addition to the default `adapter` option:

Parameter    | Description                    | Expected Type | Example Value
-------------|--------------------------------|---------------|--------------
`proxy_host` | Proxy server address           | string        | 'proxy.myhost.com'’ or '10.1.2.3'
`proxy_port` | Proxy server TCP port          | integer       | 8080 (default) or 81
`proxy_user` | Proxy user name, if required   | string        | 'shahar' or '' for none (default)
`proxy_pass` | Proxy password, if required    | string        | 'secret' or '' for none (default)
`proxy_auth` | Proxy HTTP authentication type | string        | `Zend\Http\Client::AUTH_BASIC` (default)

`proxy_host` should always be set; if it is not set, the client will fall back
to a direct connection using `Zend\Http\Client\Adapter\Socket`. `proxy_port`
defaults to '8080'; if your proxy listens on a different port, you must set this
one as well.

`proxy_user` and `proxy_pass` are only required if your proxy server requires
you to authenticate.  Providing these will add a 'Proxy-Authentication' header
to the request. If your proxy does not require authentication, you can leave
these two options out.

`proxy_auth` sets the proxy authentication type, if your proxy server requires
authentication.  Possible values are similar to the ones accepted by the
`Zend\Http\Client::setAuth()` method. Currently, only basic authentication
(`Zend\Http\Client::AUTH_BASIC`) is supported.

### Using Zend\\Http\\Client behind a proxy server

```php
use Zend\Http\Client;

// Set the configuration parameters
$config = [
    'adapter'    => Client\Adapter\Proxy::class,
    'proxy_host' => 'proxy.int.zend.com',
    'proxy_port' => 8000,
    'proxy_user' => 'shahar.e',
    'proxy_pass' => 'bananashaped',
];

// Instantiate a client object
$client = new Client('http://www.example.com', $config);

// Continue working...
```

As mentioned, if `proxy_host` is not set or is set to a blank string, the
connection will fall back to a regular direct connection. This allows you to
write your application in a way that allows a proxy to be used optionally,
according to a configuration parameter.

> ### Access to stream context
>
> Since the proxy adapter inherits from `Zend\Http\Client\Adapter\Socket`, you
> can use the stream context access method (see
> [above](#setting-stream-context-options-for-the-socket-adapter)) to set stream
> context options on `Proxy` connections.

## The cURL Adapter

cURL is a standard HTTP client library that is distributed with many operating
systems and can be used in PHP via the cURL extension. It offers functionality
for many special cases which can occur for a HTTP client and make it a perfect
choice for a HTTP adapter. It supports secure connections, proxies, and multiple
authentication mechanisms. In particular, it is very performant with regards to
transfering large files.

### Setting cURL options

```php
use Zend\Http\Client;

$config = [
    'adapter'     => Client\Adapter\Curl::class,
    'curloptions' => [CURLOPT_FOLLOWLOCATION => true],
];
$client = new Client($uri, $config);
```

By default, the cURL adapter is configured to behave exactly like the `Socket`
adapter, and it also accepts the same configuration parameters as the `Socket`
and `Proxy` adapters. You can also change the cURL options by either specifying
the 'curloptions' key in the constructor of the adapter, or by calling
`setCurlOption($name, $value)`; option names correspond to the `CURL_*`
constants of the cURL extension. You can get access to the underling cURL handle
by calling `$adapter->getHandle();`

The cURL configuration options that can be set via `setCurlOption($name, $value)`
method are:

Parameter            | Description                                                                          | Expected Type | Default Value
---------------------|--------------------------------------------------------------------------------------|---------------|--------------
`proxyuser`          | Whether to use persistent TCP connections                                            | boolean       | `FALSE`
`proxypass`          | SSL transport layer (eg. `sslv2`, `tls`)                                             | string        | `ssl`
`proxyhost`          | Path to a PEM encoded SSL certificate                                                | string        | `NULL`
`proxyport`          | Passphrase for the SSL certificate file                                              | string        | `NULL`
`sslverifypeer`      | Whether to verify the SSL peer                                                       | string        | `TRUE`

### Transfering files by handle

You can use cURL to transfer very large files over HTTP by filehandle.

```php
use Zend\Http\Client;

$putFileSize   = filesize('filepath');
$putFileHandle = fopen('filepath', 'r');

$adapter = new Client\Adapter\Curl();
$client = new Client();
$client->setAdapter($adapter);
$client->setMethod('PUT');
$adapter->setOptions([
    'curloptions' => [
        CURLOPT_INFILE     => $putFileHandle,
        CURLOPT_INFILESIZE => $putFileSize,
    ],
]);
$client->send();
```

## The Test adapter

Testing code that relies on HTTP connections poses difficulties.  For example,
testing an application that pulls an RSS feed from a remote server will require
a network connection, which is not always available.

`Zend\Http\Client\Adapter\Test` provides a solution for these situations and
acts as a mock object for unit tests. You can write your application to use
`Zend\Http\Client`, and, when testing (either in your unit test suite, or in
non-production environments), replace the default adapter with the test adapter,
allowing you to run tests without actually performing server connections.

`Zend\Http\Client\Adapter\Test` provides two additional methods, `setResponse()`
and `addResponse()`.  Each takes one parameter, which represents an HTTP
response as either text or a `Zend\Http\Response` object. `setResponse()` sets
an individual response to always return from any request; `addResponse()` allows
aggregating a sequence of responses. In both cases, responses are returned
without performing actual HTTP requests.

### Testing against a single HTTP response stub

```php
use Zend\Http\Client;

// Instantiate a new adapter and client
$adapter = new Client\Adapter\Test();
$client  = new Client(
    'http://www.example.com',
    ['adapter' => $adapter]
);

// Set the expected response
$adapter->setResponse(
    "HTTP/1.1 200 OK\r\n"
    . "Content-type: text/xml\r\n"
    . "\r\n"
    . '<?xml version="1.0" encoding="UTF-8"?>'
    . '<rss version="2.0" '
    . '     xmlns:content="http://purl.org/rss/1.0/modules/content/"'
    . '     xmlns:wfw="http://wellformedweb.org/CommentAPI/"'
    . '     xmlns:dc="http://purl.org/dc/elements/1.1/">'
    . '  <channel>'
    . '    <title>Premature Optimization</title>'
    // and so on...
    . '</rss>'
);

$response = $client->send();
// .. continue parsing $response..
```

The above example shows how you can preset your HTTP client to return the
response you need. Then, you can continue testing your own code, without being
dependent on a network connection, the server's response, etc. In this case, the
test would continue to check how the application parses the XML in the response
body.

Sometimes, a single method call to an object can result in that object
performing multiple HTTP transactions. In this case, it's not possible to use
`setResponse()` alone because there's no opportunity to set the next response(s)
your program might need before returning to the caller.

### Testing Against Multiple HTTP Response Stubs

```php
use Zend\Http\Client;

// Instantiate a new adapter and client
$adapter = new Client\Adapter\Test();
$client = new Client(
    'http://www.example.com',
    ['adapter' => $adapter]
);

// Set the first expected response
$adapter->setResponse(
    "HTTP/1.1 302 Found\r\n"
    . "Location: /\r\n"
    . "Content-Type: text/html\r\n"
    . "\r\n"
    . '<html>'
    . '  <head><title>Moved</title></head>'
    . '  <body><p>This page has moved.</p></body>'
    . '</html>'
);

// Set the next successive response
$adapter->addResponse(
    "HTTP/1.1 200 OK\r\n"
    . "Content-Type: text/html\r\n"
    . "\r\n"
    . '<html>'
    . '  <head><title>My Pet Store Home Page</title></head>'
    . '  <body><p>...</p></body>'
    . '</html>'
);

// inject the http client object ($client) into your object
// being tested and then test your object's behavior below
```

The `setResponse()` method clears any responses in the adapter's buffer and sets
the first response that will be returned. The `addResponse()` method will add
successive responses.

The responses will be replayed in the order that they were added. If more
requests are made than the number of responses stored, the responses will cycle
again in order.

In the example above, the adapter is configured to test your object's behavior
when it encounters a 302 redirect. Depending on your application, following a
redirect may or may not be desired behavior. In our example, we expect that the
redirect will be followed and we configure the test adapter to help us test
this. The initial 302 response is set up with the `setResponse()` method and the
200 response to be returned next is added with the `addResponse()` method. After
configuring the test adapter, inject the HTTP client containing the adapter into
your object under test and test its behavior.

### Forcing the adapter to fail

If you need the adapter to fail on demand you can use
`setNextRequestWillFail($flag)`. The method will cause the next call to
`connect()` to throw an `Zend\Http\Client\Adapter\Exception\RuntimeException`.
This can be useful when our application caches content from an external site (in
case the site goes down) and you want to test this feature.

```php
use Zend\Http\Client;

// Instantiate a new adapter and client
$adapter = new Client\Adapter\Test();
$client  = new Client(
    'http://www.example.com',
    ['adapter' => $adapter]
);

// Force the next request to fail with an exception
$adapter->setNextRequestWillFail(true);

try {
    // This call will result in an exception.
    $client->send();
} catch (Client\Adapter\Exception\RuntimeException $e) {
    // ...
}

// Further requests will work as expected until
// you call setNextRequestWillFail(true) again
```

## Creating your own connection adapters

`Zend\Http\Client` has been designed so that you can create and use your own
connection adapters. You could, for example, create a connection adapter that
uses persistent sockets, or a connection adapter with caching abilities, and use
them as needed in your application.

In order to do so, you must create your own adapter class that implements
`Zend\Http\Client\Adapter\AdapterInterface`. The following example shows the
skeleton of a user-implemented adapter class. All the public functions defined
in this example must be defined in your adapter as well.

```php
namespace MyApp\Http\Client\Adapter;

use Zend\Http\Client\Adapter\AdapterInterface;

class BananaProtocol implements AdapterInterface
{
    /**
     * Set Adapter Options
     *
     * @param array $config
     */
    public function setOptions($config = [])
    {
        // This rarely changes - you should usually copy the
        // implementation in Zend\Http\Client\Adapter\Socket.
    }

    /**
     * Connect to the remote server
     *
     * @param string  $host
     * @param int     $port
     * @param boolean $secure
     */
    public function connect($host, $port = 80, $secure = false)
    {
        // Set up the connection to the remote server
    }

    /**
     * Send request to the remote server
     *
     * @param string        $method
     * @param Zend\Uri\Http $url
     * @param string        $http_ver
     * @param array         $headers
     * @param string        $body
     * @return string Request as text
     */
    public function write(
        $method,
        $url,
        $http_ver = '1.1',
        $headers = [],
        $body = ''
    ) {
        // Send request to the remote server.
        // This function is expected to return the full request
        // (headers and body) as a string
    }

    /**
     * Read response from server
     *
     * @return string
     */
    public function read()
    {
        // Read response from remote server and return it as a string
    }

    /**
     * Close the connection to the server
     *
     */
    public function close()
    {
        // Close the connection to the remote server - called last.
    }
}
```

Use the adapter as you would any other:

```php
use MyApp\Http\Client\Adapter\BananaProtocol;
use Zend\Http\Client;

$client = new Client([
    'adapter' => BananaProtocol::class,
]);
```

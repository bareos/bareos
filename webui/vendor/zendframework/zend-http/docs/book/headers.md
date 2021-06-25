# Headers

`Zend\Http\Headers` is a container for HTTP headers. It is typically accessed as
part of a `Zend\Http\Request` or `Zend\Http\Response` instance, via a
`getHeaders()` call. The `Headers` container will lazily load actual
`Zend\Http\Header\HeaderInterface` instances as to reduce the overhead of header
specific parsing.

The class under the `Zend\Http\Header` namespace are the domain specific
implementations for the various types of headers that one might encounter during
the typical HTTP request. If a header of unknown type is encountered, it will be
implemented as a `Zend\Http\Header\GenericHeader` instance. See the below table
for a list of the various HTTP headers and the API that is specific to each
header type.

## Quick Start

The quickest way to get started interacting with header objects is by retrieving
an already populated `Headers` container from a request or response instance.

```php
// $client is an instance of Zend\Http\Client

// You can retrieve the request headers by first retrieving
// the Request object and then calling getHeaders on it
$requestHeaders  = $client->getRequest()->getHeaders();

// The same method also works for retrieving Response headers
$responseHeaders = $client->getResponse()->getHeaders();
```

`Zend\Http\Headers` can also extract headers from a string:

```php
use Zend\Http\Headers;

$headerString = <<<EOB
Host: www.example.com
Content-Type: text/html
Content-Length: 1337
EOB;

$headers = Headers::fromString($headerString);
// $headers is now populated with three objects
//   (1) Zend\Http\Header\Host
//   (2) Zend\Http\Header\ContentType
//   (3) Zend\Http\Header\ContentLength
```

Now that you have an instance of `Zend\Http\Headers`, you can manipulate the
individual headers it contains using the provided public API methods outlined in
the [Available Methods](#available-methods) section.

## Configuration Options

No configuration options are available.

## Available Methods

The following is a list of methods available to `Zend\Http\Headers`. For
brevity, we map the following references to the following classes or namespaces:

- `HeaderInterface`: `Zend\Http\Header\HeaderInterface`
- `Headers`: `Zend\Http\Headers`
- `PluginClassLocator`: `Zend\Loader\PluginClassLocator`

Method signature                                                          | Description
------------------------------------------------------------------------- | -----------
`static fromString(string $string) : Headers`                             | Parses a string for headers, and aggregates them, in order, a new `Headers` instance, primarily as strings until they are needed (they will be lazy loaded).
`setPluginClassLoader(PluginClassLocator $pluginClassLoader) : self`      | Set an alternate implementation for the plugin class loader.
`getPluginClassLoader() : PluginClassLocator`                             | Return an instance of a `PluginClassLocator`; lazy-load and inject map if necessary.
`addHeaders(array|Traversable $headers) : self`                           | Add many headers at once; expects an array (or `Traversable` object) of type/value pairs.
`addHeaderLine(string $headerFieldNameOrLine, string $fieldValue) : self` | Add a raw header line, either as separate name and value arguments, or as a single string in the form `name: value` This method allows for lazy-loading in that the parsing and instantiation of a `HeaderInterface` implementation will be delayed until they are retrieved by either `get()` or `current()`.
`addHeader(HeaderInterface $header) : self`                               | Add a header instance to the container; for raw values see `addHeaderLine()` and `addHeaders()`.
`removeHeader(HeaderInterface $header) : bool`                            | Remove a Header from the container.
`clearHeaders() : self`                                                   | Removes all headers from the container.
`get(string $name) : false|HeaderInterface|ArrayIterator`                 | Get all values for a given header. If none are found, `false` is returned. If the header is a single-value header, a `HeaderInterface` is returned. If the header is a multi-value header, an `ArrayIterator` containing all values is returned.
`has(string $name) : bool`                                                | Test for existence of a header.
`next() : void`                                                           | Advance the pointer for this object as an iterator.
`key() : mixed`                                                           | Return the current key for this object as an iterator.
`valid() : bool`                                                          | Is this iterator still valid?
`rewind() : void`                                                         | Reset the internal pointer for this object as an iterator.
`current() : HeaderInterface`                                             | Return the current value for this iterator, lazy loading it if need be.
`count() : int`                                                           | Return the number of headers in this container. If all headers have not been parsed, the actual count could decrease if `MultipleHeader` instances exist. If you need an exact count, iterate.
`toString() : string`                                                     | Render all headers at once.  This method handles the normal iteration of headers; it is up to the concrete classes to prepend with the appropriate status/request line.
`toArray() : array`                                                       | Return all headers as an associative array.
`forceLoading() : bool`                                                   | By calling this, it will force parsing and loading of all headers, ensuring `count()` is accurate.

## HeaderInterface Methods

The following are methods available to all `HeaderInterface` implementations.

Method signature                                          | Description
--------------------------------------------------------- | -----------
`static fromString(string $headerLine) : HeaderInterface` | Factory to generate a header object from a string.
`getFieldName() : string`                                 | Retrieve header field name.
`getFieldValue() : string`                                | Retrieve header field value.
`toString() : string`                                     | Cast the header to a well-formed HTTP header line (`Name: Value`).

## AbstractAccept Methods

`Zend\Http\Header\AbstractAccept` defines the following methods in addition to
those it inherits from the [HeaderInterface](#headerinterface-methods). The
`Accept`, `AcceptCharset`, `AcceptEncoding`, and `AcceptLanguage` header types
inherit from it.

For brevity, we map the following references to the following classes or
namespaces:

- `AcceptFieldValuePart`: `Zend\Http\Header\Accept\FieldValuePart\AcceptFieldValuePart`
- `InvalidArgumentException`: `Zend\Http\Header\Exception\InvalidArgumentException`

Method signature                                                | Description
--------------------------------------------------------------- | -----------
`parseHeaderLine(string $headerLine) : void`                    | Parse the given header line and add the values discovered to the instance.
`getFieldValuePartsFromHeaderLine(string $headerLine) : array`  | Parse the field value parts represented by an Accept* header line.  Throws `InvalidArgumentException` if the header is invalid.
`getFieldValue(array|null $values = null) : string`             | Get field value.
`match(array|string $matchAgainst) : bool|AcceptFieldValuePart` | Match a media string against this header. Returns the matched value or false.
`getPrioritized() : array`                                      | Returns all the keys, values and parameters this header represents.

## AbstractDate Methods

`Zend\Http\Header\AbstractDate` defines the following methods in addition to
those it inherits from the [HeaderInterface](#headerinterface-methods). The
`Date`, `Expires`, `IfModifiedSince`, `IfUnmodifiedSince`, `LastModified`, and
`RetryAfter` header types inherit from it.

For brevity, we map the following references to the following classes or
namespaces:

- `InvalidArgumentException`: `Zend\Http\Header\Exception\InvalidArgumentException`

Method signature                                     | Description
---------------------------------------------------- | -----------
`static fromTimestamp(int $time) : AbstractDate`     | Create date-based header from Unix timestamp.
`static fromTimeString(string $time) : AbstractDate` | Create date-based header from `strtotime()`-compatible string.
`static setDateFormat(int $format) : void`           | Set date output format; should be an index from the implementation's `$dateFormat` static property.
`static getDateFormat() : string`                    | Return current date output format.
`setDate(string|DateTime $date) : self`              | Set the date for this header; this can be a string or an instance of `DateTime`.  Throws `InvalidArgumentException` if the date is neither a valid string nor an instance of `DateTime`.
`getDate() : string`                                 | Return string representation of the date for this header.
`compareTo(string|DateTime $date) : int`             | Compare provided date to date for this header. Returns `< 0` if date in header is less than `$date`; `> 0` if it's greater, and `= 0` if they are equal. See [strcmp](http://www.php.net/manual/en/function.strcmp.php).
`date() | DateTime`                                  | Return date for this header as an instance of `DateTime`.

## AbstractLocation Methods

`Zend\Http\Header\AbstractLocation` defines the following methods in addition to
those it inherits from the [HeaderInterface](#headerinterface-methods). The
`ContentLocation`, `Location`, and `Referer` header types inherit from it.

For brevity, we map the following references to the following classes or
namespaces:

- `Uri`: `Zend\Uri\UriInterface`
- `InvalidArgumentException`: `Zend\Http\Header\Exception\InvalidArgumentException`

Method signature                 | Description
-------------------------------- | -----------
`setUri(string|Uri $uri) : self` | Set the URI for this header; throws `InvalidArgumentException` for invalid `$uri` arguments.
`getUri() : string`              | Return the URI for this header.
`uri() : Uri`                    | Return the `Uri` instance for this header.

## List of HTTP Header Types

Some header classes expose methods for manipulating their value. The following
list contains all of the classes available in the `Zend\Http\Header\*`
namespace, as well as any specific methods they contain. Each class implements
`Zend\Http\Header\HeaderInterface`.

### Accept

Extends [AbstractAccept](#abstractaccept-methods).

Method signature                                             | Description
------------------------------------------------------------ | -----------
`addMediaType(string $type, int|float $priority = 1) : self` | Add a media type, with the given priority.
`hasMediaType(string $type): bool`                           | Does the header have the requested media type?

### AcceptCharset

Extends [AbstractAccept](#abstractaccept-methods).

Method signature                                           | Description
---------------------------------------------------------- | -----------
`addCharset(string $type, int|float $priority = 1) : self` | Add a charset, with the given priority.
`hasCharset(string $type) : bool`                          | Does the header have the requested charset?

### AcceptEncoding

Extends [AbstractAccept](#abstractaccept-methods).

Method signature                                            | Description
----------------------------------------------------------- | -----------
`addEncoding(string $type, int|float $priority = 1) : self` | Add an encoding, with the given priority.
`hasEncoding(string $type) : bool`                          | Does the header have the requested encoding?

### AcceptLanguage

Extends [AbstractAccept](#abstractaccept-methods).

Method signature                                           | Description
---------------------------------------------------------- | -----------
`addLanguage(string $type, int|float $priority = 1): self` | Add a language, with the given priority.
`hasLanguage(string $type) : bool`                         | Does the header have the requested language?

### AcceptRanges

Method signature                        | Description
--------------------------------------- | -----------
`getRangeUnit() : mixed`                | (unknown)
`setRangeUnit(mixed $rangeUnit) : self` | (unkown)

### Age

Method signature                       | Description
-------------------------------------- | -----------
`getDeltaSeconds() : int`              | Get number of seconds.
`setDeltaSeconds(int $seconds) : self` | Set number of seconds

### Allow

Method signature                                     | Description
---------------------------------------------------- | -----------
`getAllMethods() : string[]`                         | Get list of all defined methods.
`getAllowedMethods() : string[]`                     | Get list of allowed methods.
`allowMethods(array|string $allowedMethods) : self`  | Allow methods or list of methods.
`disallowMethods(array|string $allowedMethods) self` | Disallow methods or list of methods.
`denyMethods(array|string $allowedMethods) : self`   | Convenience alias for `disallowMethods()`.
`isAllowedMethod(string $method) : bool`             | Check whether method is allowed.

### AuthenticationInfo

No additional methods.

### Authorization

No additional methods.

### CacheControl

Method signature                                              | Description
------------------------------------------------------------- | -----------
`isEmpty(): bool`                                             | Checks if the internal directives array is empty.
`addDirective(string $key, string|bool $value = true) : self` | Add a directive. For directives like `max-age=60`, call as `addDirective('max-age', 60)`. For directives like `private`, use the default `$value` (`true`).
`hasDirective(string $key) : bool`                            | Check the internal directives array for a directive.
`getDirective(string $key) : null|string`                     | Fetch the value of a directive from the internal directive array.
`removeDirective(string $key) : self`                         | Remove a directive.

### Connection

Method signature                   | Description
---------------------------------- | -----------
`setValue($value) : self`          | Set arbitrary header value.  RFC allows any token as value; 'close' and 'keep-alive' are commonly used.
`isPersistent() : bool`            | Whether or not the connection is persistent.
`setPersistent(bool $flag) : self` | Set Connection header to define persistent connection.

### ContentDisposition

No additional methods.

### ContentEncoding

No additional methods.

### ContentLanguage

No additional methods.

### ContentLength

No additional methods.

### ContentLocation

See [AbstractLocation](#abstractlocation-methods).

### ContentMD5

No additional methods.

### ContentRange

No additional methods.

### ContentSecurityPolicy

Method signature                                    | Description
--------------------------------------------------- | -----------
`getDirectives(): array`                            | Retrieve the defined directives for the policy.
`setDirective(string $name, array $sources) : self` | Set the directive with the given name to include the sources. See below for an example.

As an example: an auction site wishes to load images from any URI, plugin
content from a list of trusted media providers (including a content distribution
network), and scripts only from a server under its control hosting sanitized
Javacript:

```php
// http://www.w3.org/TR/2012/CR-CSP-20121115/#sample-policy-definitions
$csp = new ContentSecurityPolicy();
$csp->setDirective('default-src', []); // No sources
$csp->setDirective('img-src', ['*']);
$csp->setDirective('object-src', ['media1.example.com', 'media2.example.com', '*.cdn.example.com']);
$csp->setDirective('script-src', ['trustedscripts.example.com']);
```

### ContentTransferEncoding

No additional methods.

### ContentType

Method signature                                  | Description
------------------------------------------------- | -----------
`match(array|string $matchAgainst) : bool|string` | Determine if the mediatype value in this header matches the provided criteria.
`getMediaType() : string`                         | Get the media type.
`setMediaType(string $mediaType) : self`          | Set the media type.
`getParameters() : array`                         | Get any additional content-type parameters currently set.
`setParameters(array $parameters) : self`         | Set additional content-type parameters.
`getCharset() : null|string`                      | Get the content-type character set encoding, if any.
`setCharset(string $charset) : self`              | Set the content-type character set encoding.

### Cookie

Extends `ArrayObject`.

Method signature                                        | Description
------------------------------------------------------- | -----------
`static fromSetCookieArray(array $setCookies) : Cookie` | Create an instance from the `$_COOKIE` array, or one structured like it.
`setEncodeValue(bool $encode) : self`                   | Set flag indicating whether or not to `urlencode()` the cookie values.
`getEncodeValue() : bool`                               | Get flag indicating whether or not to `urlencode()` the cookie values.

### Date

See [AbstractDate](#abstractdate-methods).

### Etag

No additional methods.

### Expect

No additional methods.

### Expires

See [AbstractDate](#abstractdate-methods).

### From

No additional methods.

### Host

No additional methods.

### IfMatch

No additional methods.

### IfModifiedSince

See [AbstractDate](#abstractdate-methods).

### IfNoneMatch

No additional methods.

### IfRange

No additional methods.

### IfUnmodifiedSince

See [AbstractDate](#abstractdate-methods).

### KeepAlive

No additional methods.

### LastModified

See [AbstractDate](#abstractdate-methods).

### Location

See [AbstractLocation](#abstractlocation-methods).

### MaxForwards

No additional methods.

### Origin

No additional methods.

### Pragma

No additional methods.

### ProxyAuthenticate

Method signature                                   | Description
-------------------------------------------------- | -----------
`toStringMultipleHeaders(array $headers) : string` | Creates a string representation when multiple values are present.

### ProxyAuthorization

No additional methods.

### Range

No additional methods.

### Referer

See [AbstractLocation](#abstractlocation-methods).

### Refresh

No additional methods.

### RetryAfter

See [AbstractDate](#abstractdate-methods).

Method signature                     | Description
------------------------------------ | -----------
`setDeltaSeconds(int $delta) : self` | Set number of seconds.
`getDeltaSeconds() : int`            | Get number of seconds.

### Server

No additional methods.

### SetCookie

Method signature                                                      | Description
--------------------------------------------------------------------- | -----------
`static matchCookieDomain(string $cookieDomain, string $host) : bool` | Check if a cookie's domain matches a host name.
`static matchCookiePath(string $cookiePath, string $path) : bool`     | Check if a cookie's path matches a URL path.
`getName() : string`                                                  | Retrieve the cookie name.
`setName(string $name) : self`                                        | Set the cookie name.
`getValue() : string`                                                 | Retrieve the cookie value.
`setValue(string $value) : self`                                      | Set the cookie value.
`getExpires() : int`                                                  | Retrieve the expiration date for the cookie.
`setExpires(int|string $expires) : self`                              | Set the cookie expiration timestamp; null indicates a session cookie.
`getPath() : string`                                                  | Retrieve the URI path the cookie is bound to.
`setPath(string $path) : self`                                        | Set the URI path the cookie is bound to.
`getDomain() : string`                                                | Retrieve the domain the cookie is bound to.
`setDomain(string $domain) : self`                                    | Set the domain the cookie is bound to.
`getMaxAge() : int`                                                   | Retrieve the maximum age for the cookie.
`setMaxAge(int|string $maxAge) : self`                                | Set the maximum age for the cookie.
`getVersion() : int`                                                  | Retrieve the cookie version.
`setVersion(int|string $version) : self`                              | Set the cookie version.
`isSecure(): bool`                                                    | Whether the cookies contains the Secure flag.
`setSecure(bool $secure) : self`                                      | Set whether the cookies contain the Secure flag.
`isHttponly() : bool`                                                 | Whether the cookies can be accessed via the HTTP protocol only.
`setHttponly(bool $httponly) : self`                                  | Set whether the cookies can be accessed only via HTTP protocol.
`isExpired() : bool`                                                  | Whether the cookie is expired.
`isSessionCookie() : bool`                                            | Whether the cookie is a session cookie.
`setQuoteFieldValue(bool $quotedValue) : self`                        | Set whether the value for this cookie should be quoted.
`hasQuoteFieldValue() : bool`                                         | Check whether the value for this cookie should be quoted.
`isValidForRequest() : bool`                                          | Whether the cookie is valid for a given request domain, path and isSecure.
`match(string $uri, bool $matchSessionCookies, int $now) : bool`      | Checks whether the cookie should be sent or not in a specific scenario.
`toStringMultipleHeaders(array $headers) : string`                    | Returns string representation when multiple values are present.

### TE

No additional methods.

### Trailer

No additional methods.

### TransferEncoding

No additional methods.

### Upgrade

No additional methods.

### UserAgent

No additional methods.

### Vary

No additional methods.

### Via

No additional methods.

### Warning

No additional methods.

### WWWAuthenticate

Defines a `toStringMultipleHeaders(array $headers)` method for serializing to
string when multiple values are present.

## Examples

### Retrieving headers from a Headers object

```php
// $client is an instance of Zend\Http\Client
$response = $client->send();
$headers = $response->getHeaders();

// We can check if the Request contains a specific header by
// using the ``has`` method. Returns boolean ``TRUE`` if at least
// one matching header found, and ``FALSE`` otherwise
$headers->has('Content-Type');

// We can retrieve all instances of a specific header by using
// the ``get`` method:
$contentTypeHeaders = $headers->get('Content-Type');
```

There are three possibilities for the return value of the above call to the `get` method:

- If no `Content-Type` header was set in the `Request`, `get` will return `false`.
- If only one `Content-Type` header was set in the `Request`, `get` will return
  an instance of `Zend\Http\Header\ContentType`.
- If more than one `Content-Type` header was set in the `Request`, `get` will
  return an `ArrayIterator` containing one `Zend\Http\Header\ContentType`
  instance per header.

### Adding headers to a Headers object

```php
use Zend\Http\Header;
use Zend\Http\Headers;

$headers = new Headers();

// We can directly add any object that implements Zend\Http\Header\HeaderInterface
$typeHeader = Header\ContentType::fromString('Content-Type: text/html');
$headers->addHeader($typeHeader);

// We can add headers using the raw string representation, either
// passing the header name and value as separate arguments...
$headers->addHeaderLine('Content-Type', 'text/html');

// .. or we can pass the entire header as the only argument
$headers->addHeaderLine('Content-Type: text/html');

// We can also add headers in bulk using addHeaders, which accepts
// an array of individual header definitions that can be in any of
// the accepted formats outlined below:
$headers->addHeaders([
    // An object implementing Header\HeaderInterface
    Header\ContentType::fromString('Content-Type: text/html'),

    // A raw header string
    'Content-Type: text/html',

    // We can also pass the header name as the array key and the
    // header content as that array key's value
    'Content-Type' => 'text/html',
]);
```

### Removing headers from a Headers object

We can remove all headers of a specific type using the `removeHeader` method,
which accepts a single object implementing `Zend\Http\Header\HeaderInterface`

```php
use ArrayIterator;
use Zend\Http\Header\HeaderInterface;

// $headers is a pre-configured instance of Zend\Http\Headers

// We can also delete individual headers or groups of headers
$matches = $headers->get('Content-Type');

if ($matches instanceof ArrayIterator) {
    // If more than one header was found, iterate over the collection
    // and remove each one individually
    foreach ($headers as $header) {
        $headers->removeHeader($header);
    }
} elseif ($matches instanceof HeaderInterface) {
    // If only a single header was found, remove it directly
    $headers->removeHeader($header);
}

// In addition to this, we can clear all the headers currently stored in
// the container by calling the clearHeaders() method
$matches->clearHeaders();
```

# The Request Class

`Zend\Http\Request` is responsible for providing a fluent API that allows a
developer to interact with all the various parts of an HTTP request.

A typical HTTP request looks like this:

```text
| METHOD | URI | VERSION |
|        HEADERS         |
|         BODY           |
```

In simplified terms, the request consists of a method, URI, and HTTP version
number; together, they make up the "Request Line." This line is followed by zero
or more HTTP headers, which is followed by an empty line and then the request
body; the body is typically used when a client wishes to send data &mdash; which
could be urlencoded parameters, a JSON document, an XML document, or even one or
more files &mdash; to the server.  More information on the structure and
specification of a HTTP request can be found in
[RFC-2616 on the W3.org site](http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html).

## Quick Start

Request objects can either be created from the provided `fromString()` factory,
or, if you wish to have a completely empty object to start with, by
manually instantiating the `Zend\Http\Request` class with no parameters.

```php
use Zend\Http\Request;

$request = Request::fromString(<<<EOS
POST /foo HTTP/1.1
HeaderField1: header-field-value1
HeaderField2: header-field-value2

foo=bar
EOS
);

// OR, the completely equivalent

$request = new Request();
$request->setMethod(Request::METHOD_POST);
$request->setUri('/foo');
$request->getHeaders()->addHeaders([
    'HeaderField1' => 'header-field-value1',
    'HeaderField2' => 'header-field-value2',
]);
$request->getPost()->set('foo', 'bar');
```

## Configuration Options

No configuration options are available.

## Available Methods

The following table details available methods, their signatures, and a brief
description. Note that the following references refer to the following
fully qualified class names and/or namespaces:

- `HeaderInterface`: `Zend\Http\Header\HeaderInterface`
- `Headers`: `Zend\Http\Headers`
- `Header`: `Zend\Http\Header`
- `Parameters`: `Zend\Stdlib\ParametersInterface`
- `Request`: `Zend\Http\Request`
- `Uri`: `Zend\Uri\Http`

Method signature                                                            | Description
--------------------------------------------------------------------------- | -----------
`static fromString(string $string) : Request`                               | A factory that produces a `Request` object from a well-formed HTTP request message string.
`setMethod(string $method) : self`                                          | Set the method for this request.
`getMethod() : string`                                                      | Return the method for this request.
`setUri(string|Uri $uri) : self`                                            | Set the URI/URL for this request; this can be a string or an instance of `Zend\Uri\Http`.
`getUri() : Uri`                                                            | Return the URI for this request object.
`getUriString() : string`                                                   | Return the URI for this request object as a string.
`setVersion(string $version) : self`                                        | Set the HTTP version for this object, one of 1.0 or 1.1 (`Request::VERSION_10`, `Request::VERSION_11`).
`getVersion() : string`                                                     | Return the HTTP version for this request.
`setQuery(Parameters $query) : self`                                        | Provide an alternate Parameter Container implementation for query parameters in this object. (This is NOT the primary API for value setting; for that, see `getQuery()`).
`getQuery(string|null $name, mixed|null $default) : null|string|Parameters` | Return the parameter container responsible for query parameters or a single query parameter based on `$name`.
`setPost(Parameters $post) : self`                                          | Provide an alternate Parameter Container implementation for POST parameters in this object. (This is NOT the primary API for value setting; for that, see `getPost()`).
`getPost(string|null $name, mixed|null $default) : null|string|Parameters`  | Return the parameter container responsible for POST parameters or a single POST parameter, based on `$name`.
`getCookie() : Header\Cookie`                                               | Return the Cookie header, this is the same as calling `$request->getHeaders()->get('Cookie');`.
`setFiles(Parameters $files) : self`                                        | Provide an alternate Parameter Container implementation for file parameters in this object, (This is NOT the primary API for value setting; for that, see `getFiles()`).
`getFiles(string|null $name, mixed|null $default) : null|string|Parameters` | Return the parameter container responsible for file parameters or a single file parameter, based on `$name`.
`setHeaders(Headers $headers) : self`                                       | Provide an alternate Parameter Container implementation for headers in this object, (this is NOT the primary API for value setting, for that see `getHeaders()`).
`getHeaders(string|null $name, mixed|null $default) : mixed`                | Return the container responsible for storing HTTP headers. This container exposes the primary API for manipulating headers set in the HTTP request. See the section on [Headers](headers.md) for more information. Return value is based on `$name`; `null` returns `Headers`, while a matched header returns a `Header\HeaderInterface` implementation for single-value headers or an `ArrayIterator` for multi-value headers.
`setMetadata(string|int|array|Traversable $spec, mixed $value) : self`      | Set message metadata.  Non-destructive setting of message metadata; always adds to the metadata, never overwrites the entire metadata container.
`getMetadata(null|string|int $key, null|mixed $default) : mixed`            | Retrieve all metadata or a single metadatum as specified by key.
`setContent(mixed $value) : self`                                           | Set request body (content).
`getContent() : mixed`                                                      | Get request body (content).
`isOptions() : bool`                                                        | Is this an OPTIONS method request?
`isGet() : bool`                                                            | Is this a GET method request?
`isHead() : bool`                                                           | Is this a HEAD method request?
`isPost() : bool`                                                           | Is this a POST method request?
`isPut() : bool`                                                            | Is this a PUT method request?
`isDelete() : bool`                                                         | Is this a DELETE method request?
`isTrace() : bool`                                                          | Is this a TRACE method request?
`isConnect() : bool`                                                        | Is this a CONNECT method request?
`isPatch() : bool`                                                          | Is this a PATCH method request?
`isXmlHttpRequest() : bool`                                                 | Is this a Javascript XMLHttpRequest?
`isFlashRequest() : bool`                                                   | Is this a Flash request?
`renderRequestLine() : string`                                              | Return the formatted request line (first line) for this HTTP request.
`toString() : string`                                                       | Returns string
`__toString() : string`                                                     | Allow PHP casting of this object.

## Examples

### Generating a Request object from a string

```php
use Zend\Http\Request;

$string = "GET /foo HTTP/1.1\r\n\r\nSome Content";
$request = Request::fromString($string);

$request->getMethod();    // returns Request::METHOD_GET
$request->getUri();       // returns Zend\Uri\Http object
$request->getUriString(); // returns '/foo'
$request->getVersion();   // returns Request::VERSION_11 or '1.1'
$request->getContent();   // returns 'Some Content'
```

### Retrieving and setting headers

```php
use Zend\Http\Request;
use Zend\Http\Header\Cookie;

$request = new Request();
$request->getHeaders()->get('Content-Type'); // return content type
$request->getHeaders()->addHeader(new Cookie(['foo' => 'bar']));
foreach ($request->getHeaders() as $header) {
    printf("%s with value %s\n", $header->getFieldName(), $header->getFieldValue());
}
```

### Retrieving and setting GET and POST values

```php
use Zend\Http\Request;

$request = new Request();

// getPost() and getQuery() both return, by default, a Parameters object, which
// extends ArrayObject
$request->getPost()->foo = 'Foo value';
$request->getQuery()->bar = 'Bar value';
$request->getPost('foo'); // returns 'Foo value'
$request->getQuery()->offsetGet('bar'); // returns 'Bar value'
```

### Generating a formatted HTTP Request from a Request object

```php
use Zend\Http\Request;

$request = new Request();
$request->setMethod(Request::METHOD_POST);
$request->setUri('/foo');
$request->getHeaders()->addHeaders([
    'HeaderField1' => 'header-field-value1',
    'HeaderField2' => 'header-field-value2',
]);
$request->getPost()->set('foo', 'bar');
$request->setContent($request->getPost()->toString());
echo $request->toString();

/** Will produce:
POST /foo HTTP/1.1
HeaderField1: header-field-value1
HeaderField2: header-field-value2

foo=bar
*/
```

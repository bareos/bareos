# The Response Class

`Zend\Http\Response` is responsible for providing a fluent API that allows a
developer to interact with all the various parts of an HTTP response.

A typical HTTP Response looks like this:

```text
| VERSION | CODE | REASON |
|        HEADERS          |
|         BODY            |
```

The first line of the response consists of the HTTP version, status code, and
the reason string for the provided status code; this is called the Response
Line. Next is a set of zero or more headers.  The remainder of the response is
the response body, which is typically a string of HTML that will render on the
client's browser, but which can also be a place for request/response payload
data typical of an AJAX request. More information on the structure and
specification of an HTTP response can be found in
[RFC-2616 on the W3.org site](http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html).

## Quick Start

Response objects can either be created from the provided `fromString()` factory,
or, if you wish to have a completely empty object to start with, by
instantiating the `Zend\Http\Response` class with no arguments.

```php
use Zend\Http\Response;
$response = Response::fromString(<<<EOS
HTTP/1.0 200 OK
HeaderField1: header-field-value
HeaderField2: header-field-value2

<html>
<body>
    Hello World
</body>
</html>
EOS);

// OR

$response = new Response();
$response->setStatusCode(Response::STATUS_CODE_200);
$response->getHeaders()->addHeaders([
    'HeaderField1' => 'header-field-value',
    'HeaderField2' => 'header-field-value2',
]);
$response->setContent(<<<EOS
<html>
<body>
    Hello World
</body>
</html>
EOS
);
```

## Configuration Options

No configuration options are available.

## Available Methods

The following table details available methods, their signatures, and a brief
description. Note that the following references refer to the following
fully qualified class names and/or namespaces:

- `Headers`: `Zend\Http\Headers`
- `Response`: `Zend\Http\Response`

Method signature                                                       | Description
---------------------------------------------------------------------- | -----------
`static fromString(string $string) : Response`                         | Populate object from string.
`renderStatusLine() : string`                                          | Render the status line header
`setHeaders(Headers $headers) : self`                                  | Provide an alternate Parameter Container implementation for headers in this object. (This is NOT the primary API for value setting; for that, see `getHeaders()`.)
`getHeaders() : Headers`                                               | Return the container responsible for storing HTTP headers. This container exposes the primary API for manipulating headers set in the HTTP response. See the section on [Headers](headers.md) for more information.
`setVersion(string $version) : self`                                   | Set the HTTP version for this object, one of 1.0 or 1.1 (`Request::VERSION_10`, `Request::VERSION_11`).
`getVersion() : string`                                                | Return the HTTP version for this request.
`setStatusCode(int $code) : self`                                      | Set HTTP status code.
`getStatusCode() : int`                                                | Retrieve HTTP status code.
`setReasonPhrase(string $reasonPhrase) : self`                         | Set custom HTTP status message.
`getReasonPhrase() : string`                                           | Get HTTP status message.
`isClientError() : bool`                                               | Does the status code indicate a client error?
`isForbidden() : bool`                                                 | Is the request forbidden due to ACLs?
`isInformational() : bool`                                             | Is the current status "informational"?
`isNotFound() : bool`                                                  | Does the status code indicate the resource is not found?
`isOk() : bool`                                                        | Do we have a normal, OK response?
`isServerError() : bool`                                               | Does the status code reflect a server error?
`isRedirect() : bool`                                                  | Do we have a redirect?
`isSuccess() : bool`                                                   | Was the response successful?
`decodeChunkedBody(string $body) : string`                             | Decode a "chunked" transfer-encoded body and return the decoded text.
`decodeGzip(string $body) : string`                                    | Decode a gzip encoded message (when `Content-Encoding` indicates gzip). Currently requires PHP with zlib support.
`decodeDeflate(string $body) : string`                                 | Decode a zlib deflated message (when `Content-Encoding` indicates deflate). Currently requires PHP with zlib support.
`setMetadata(string|int|array|Traversable $spec, mixed $value) : self` | Non-destructive setting of message metadata; always adds to the metadata, never overwrites the entire metadata container.
`getMetadata(null|string|int $key, null|mixed $default) : mixed`       | Retrieve all metadata or a single metadatum as specified by key.
`setContent(mixed $value) : self`                                      | Set message content.
`getContent() : mixed`                                                 | Get raw message content.
`getBody() : mixed`                                                    | Get decoded message content.
`toString() : string`                                                  | Returns string representation of response.

## Examples

### Generating a Response object from a string

```php
use Zend\Http\Response;
$request = Response::fromString(<<<EOS
HTTP/1.0 200 OK
HeaderField1: header-field-value
HeaderField2: header-field-value2

<html>
<body>
    Hello World
</body>
</html>
EOS);
```

### Generating a formatted HTTP Response from a Response object

```php
use Zend\Http\Response;
$response = new Response();
$response->setStatusCode(Response::STATUS_CODE_200);
$response->getHeaders()->addHeaders([
    'HeaderField1' => 'header-field-value',
    'HeaderField2' => 'header-field-value2',
]);
$response->setContent(<<<EOS
<html>
<body>
    Hello World
</body>
</html>
EOS);
```

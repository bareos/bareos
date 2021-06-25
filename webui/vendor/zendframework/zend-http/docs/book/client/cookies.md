# Client Cookies

`Zend\Http\Cookies` can be used with `Zend\Http\Client` to manage sending
cookies in the request and setting cookies from the response; it is populated
from the `Set-Cookie` headers obtained from a client response, and then used to
populate the `Cookie` headers for a client request. This is highly useful in
cases where you need to maintain a user session over consecutive HTTP requests,
automatically sending the session ID cookies when required. Additionally, the
`Zend\Http\Cookies` object can be serialized and stored in `$_SESSION` when
needed.

`Zend\Http\Client` already provides methods for managing cookies for requests;
`Zend\Http\Cookies` manages the parsing of `Set-Cookie` headers returned in the
response, and allows persisting them. Additionally, `Cookies` can return a
subset of cookies that match the current request, ensuring you are only sending
relevant cookies.

## Usage

`Cookies` is an extension of `Zend\Http\Headers`, and inherits its methods. It
can be instantiated without any arguments.

```php
use Zend\Http\Cookies;

$cookies = new Cookies();
```

On your first client request, you likely won't have any cookies, so the
instance does nothing.

Once you've made your first request, you can start using it. Populate it from
the response:

```php
$response = $client->send();

$cookies->addCookiesFromResponse($response, $client->getUri());
```

Alternately, you can create your initial `Cookies` instance using the static `fromResponse()` method:

```php
$cookies = Cookies::fromResponse($response, $client->getUri());
```

On subsequent requests, we'll notify the client of our cookies. To do this, we
should use the same URI we'll use for the request.

```php
$client->setUri($uri);
$client->setCookies($cookies->getMatchingCookies($uri));
```

After the request, don't forget to add any cookies returned!

Essentially, `Cookies` aggregates all cookies for our client interactions, and
allows us to send only those relevant to a given request.

## Serializing and caching cookies

To cache cookies &mdash; e.g., to store in `$_SESSION`, or between job
invocations &mdash; you will need to serialize them. `Zend\Http\Cookies`
provides this functionality via the `getAllCookies()` method.

If your cache storage allows array structures, use the `COOKIE_STRING_ARRAY` constant:

```php
$cookiesToCache = $cookies->getAllCookies($cookies::COOKIE_STRING_ARRAY);
```

If your cache storage only allows string values, use `COOKIE_STRING_CONCAT`:

```php
$cookiesToCache = $cookies->getAllCookies($cookies::COOKIE_STRING_CONCAT);
```

When you retrieve the value later, you can test its type to determine how to
deserialize the values:

```php
use Zend\Http\Cookies;
use Zend\Http\Headers;

$cookies = new Cookies();

if (is_array($cachedCookies)) {
    foreach ($cachedCookies as $cookie) {
        $cookies->addCookie($cookie);
    }
} elseif (is_string($cachedCookies)) {
    foreach (Headers::fromString($cachedCookies) as $cookie) {
        $cookies->addCookie($cookie);
    }
}
```

## Public methods

Besides the methods demonstrated in the examples, `Zend\Http\Cookies` defines the following:

Method signature                                                    | Description
------------------------------------------------------------------- | -----------
`static fromResponse(Response $response, string $refUri) : Cookies` | Create a `Cookies` instance from a response and the request URI. Parses all `Set-Cookie` headers, maps them to the URI, and aggregates them.
`addCookie(string|SetCookie $cookie, string $refUri = null) : void` | Add a cookie, mapping it to the given URI. If no URI is provided, it will be inferred from the cookie value's domain and path.
`addCookiesFromResponse(Response $response, string $refUri) : void` | Add all `Set-Cookie` values from the provided response, mapping to the given URI.
`getAllCookies(int $retAs = self::COOKIE_OBJECT) : array|string`    | Retrieve all cookies. Returned array will have either `SetCookie` instances (the default), strings for each `Set-Cookie` declaration, or a single string containing all declarations, based on the `COOKIE_*` constant used.
`getCookie(/* ... */) : string|SetCookie`                           | Retrieve a single cookie by name for the given URI. See below for argument details.
`getMatchingCookies(/* ... */) : array`                             | See below for details.
`isEmpty() : bool`                                                  | Whether or not the instance aggregates any cookies currently.
`reset() : void`                                                    | Clear all aggregated cookies from the instance.

`getCookie()` accepts the following arguments, in the following order:

Argument                           | Description
---------------------------------- | -----------
`string $uri`                      | URI to match when retrieving the cookie. Will use its protocol, domain, and path.
`string $cookieName`               | The specific cookie name to retrieve.
`int $retAs = self::COOKIE_OBJECT` | How to return matched cookies; defaults to `SetCookie` objects. Can be any of the `Cookies::COOKIE_*` constant values.

`getMatchingCookies()` accepts the following arguments, in the following order:

Argument                           | Description
---------------------------------- | -----------
`string $uri`                      | URI to match when retrieving cookies. Will use its protocol, domain, and path.
`bool $matchSessionCookies = true` | Whether or not to also return related session cookies.
`int $retAs = self::COOKIE_OBJECT` | How to return matched cookies; defaults to `SetCookie` objects. Can be any of the `Cookies::COOKIE_*` constant values.
`int $now = null`                  | Timestamp against which to match; defaults to `time()`. Any expired cookies will be ignored.

# Introduction

zend-http provides the HTTP message abstraction used by
[zend-mvc](https://docs.zendframework.com/zend-mvc/), and also provides an
extensible, adapter-driven HTTP client library. It provides the following
abstractions:

- Context-less `Request` and `Response` classes that expose a fluent API for
  introspecting several aspects of HTTP messages:
  - Request line information and response status information
  - Parameters, such as those found in POST and GET
  - Message Body
  - Headers
- A client implementation with various adapters that allow for sending requests
  and introspecting responses.

> ### Not PSR-7!
>
> This library **does not** support [PSR-7](http://www.php-fig.org/psr/psr-7), as
> it predates that specification. For PSR-7 support, please see our
> [Diactoros component](https://docs.zendframework.com/zend-diactoros/).

## Zend\Http Request, Response and Headers

The request, response and headers implementations of the zend-http component
provides a fluent, object-oriented interface for introspecting information from
all the various parts of an HTTP request or HTTP response. The primary classes
are `Zend\Http\Request` and `Zend\Http\Response`. Both are “context-less”,
meaning that they model a request or response in the same way whether it is
presented by a client (to **send** a request and **receive** a response) or by a
server (to **receive** a request and **send** a response). In other words,
regardless of the context, the API remains the same for introspecting their
various respective parts. Each attempts to fully model a request or response so
that a developer can create these objects from a factory, or create and populate
them manually.

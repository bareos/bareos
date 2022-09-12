# Routing

Routing is the act of matching a request to a given controller.

Typically, routing will examine the request URI, and attempt to match the URI path segment against
provided constraints. If the constraints match, a set of "matches" are returned, one of which should
be the controller name to execute. Routing can utilize other portions of the request URI or
environment as well -- for example, the host or scheme, query parameters, headers, request method,
and more.

Routing has been written from the ground up for Zend Framework 2.0. Execution is quite similar, but
the internal workings are more consistent, performant, and often simpler.

> ## Note
If you are a developer with knowledge of the routing system in Zend Framework 1.x, you should know
that some of the old terminology does not apply in Zend Framework 2.x. In the new routing system we
don't have a router as such, as every route can match and assemble URIs by themselves, which makes
them routers, too.
That said, in most cases the developer does not need to worry about this, because Zend Framework 2.x
will take care of this "under the hood". The work of the router will be done by
`Zend\Mvc\Router\SimpleRouteStack` or `Zend\Mvc\Router\Http\TreeRouteStack`.

The base unit of routing is a `Route`:

```php
namespace Zend\Mvc\Router;

use Zend\Stdlib\RequestInterface as Request;

interface RouteInterface
{
    public static function factory(array $options = array());
    public function match(Request $request);
    public function assemble(array $params = array(), array $options = array());
}
```

A `Route` accepts a `Request`, and determines if it matches. If so, it returns a `RouteMatch`
object:

```php
namespace Zend\Mvc\Router;

class RouteMatch
{
    public function __construct(array $params);
    public function setMatchedRouteName($name);
    public function getMatchedRouteName();
    public function setParam($name, $value);
    public function getParams();
    public function getParam($name, $default = null);
}
```

Typically, when a `Route` matches, it will define one or more parameters. These are passed into the
`RouteMatch`, and objects may query the `RouteMatch` for their values.

```php
$id = $routeMatch->getParam('id', false);
if (!$id) {
    throw new Exception('Required identifier is missing!');
}
$entity = $resource->get($id);
```

Usually you will have multiple routes you wish to test against. In order to facilitate this, you
will use a route aggregate, usually implementing `RouteStack`:

```php
namespace Zend\Mvc\Router;

interface RouteStackInterface extends RouteInterface
{
    public function addRoute($name, $route, $priority = null);
    public function addRoutes(array $routes);
    public function removeRoute($name);
    public function setRoutes(array $routes);
}
```

Typically, routes should be queried in a LIFO order, and hence the reason behind the name
`RouteStack`. Zend Framework provides two implementations of this interface, `SimpleRouteStack` and
`TreeRouteStack`. In each, you register routes either one at a time using `addRoute()`, or in bulk
using `addRoutes()`.

```php
// One at a time:
$route = Literal::factory(array(
    'route' => '/foo',
    'defaults' => array(
        'controller' => 'foo-index',
        'action'     => 'index',
    ),
));
$router->addRoute('foo', $route);

// In bulk:
$router->addRoutes(array(
    // using already instantiated routes:
    'foo' => $route,

    // providing configuration to allow lazy-loading routes:
    'bar' => array(
        'type' => 'literal',
        'options' => array(
            'route' => '/bar',
            'defaults' => array(
                'controller' => 'bar-index',
                'action'     => 'index',
            ),
        ),
    ),
));
```

## Router Types

Two routers are provided, the `SimpleRouteStack` and `TreeRouteStack`. Each works with the above
interface, but utilize slightly different options and execution paths. By default, the `Zend\Mvc`
uses the `TreeRouteStack` as the router.

### SimpleRouteStack

This router simply takes individual routes that provide their full matching logic in one go, and
loops through them in LIFO order until a match is found. As such, routes that will match most often
should be registered last, and least common routes first. Additionally, you will need to ensure that
routes that potentially overlap are registered such that the most specific match will match first
(i.e., register later). Alternatively, you can set priorities by giving the priority as third
parameter to the `addRoute()` method, specifying the priority in the route specifications or setting
the priority property within a route instance before adding it to the route stack.

### TreeRouteStack

`Zend\Mvc\Router\Http\TreeRouteStack` provides the ability to register trees of routes, and will use
a B-tree algorithm to match routes. As such, you register a single route with many children.

A `TreeRouteStack` will consist of the following configuration:

- A base "route", which describes the base match needed, the root of the tree.
- An optional "route\_plugins", which is a configured `Zend\Mvc\Router\RoutePluginManager` that can
lazy-load routes.
- The option "may\_terminate", which hints to the router that no other segments will follow it.
- An optional "child\_routes" array, which contains additional routes that stem from the base
"route" (i.e., build from it). Each child route can itself be a `TreeRouteStack` if desired; in
fact, the `Part` route works exactly this way.

When a route matches against a `TreeRouteStack`, the matched parameters from each segment of the
tree will be returned.

A `TreeRouteStack` can be your sole route for your application, or describe particular path segments
of the application.

An example of a `TreeRouteStack` is provided in the documentation of the `Part` route.

## HTTP Route Types

Zend Framework 2.0 ships with the following HTTP route types.

### Zend\\Mvc\\Router\\Http\\Hostname

The `Hostname` route attempts to match the hostname registered in the request against specific
criteria. Typically, this will be in one of the following forms:

- "subdomain.domain.tld"
- ":subdomain.domain.tld"

In the above, the second route would return a "subdomain" key as part of the route match.

For any given hostname segment, you may also provide a constraint. As an example, if the "subdomain"
segment needed to match only if it started with "fw" and contained exactly 2 digits following, the
following route would be needed:

```php
$route = Hostname::factory(array(
    'route' => ':subdomain.domain.tld',
    'constraints' => array(
        'subdomain' => 'fw\d{2}',
    ),
));
```

In the above example, only a "subdomain" key will be returned in the `RouteMatch`. If you wanted to
also provide other information based on matching, or a default value to return for the subdomain,
you need to also provide defaults.

```php
$route = Hostname::factory(array(
    'route' => ':subdomain.domain.tld',
    'constraints' => array(
        'subdomain' => 'fw\d{2}',
    ),
    'defaults' => array(
        'type' => 'json',
    ),
));
```

When matched, the above will return two keys in the `RouteMatch`, "subdomain" and "type".

### Zend\\Mvc\\Router\\Http\\Literal

The `Literal` route is for doing exact matching of the URI path. Configuration therefore is solely
the path you want to match, and the "defaults", or parameters you want returned on a match.

```php
$route = Literal::factory(array(
    'route' => '/foo',
    'defaults' => array(
        'controller' => 'Application\Controller\IndexController',
        'action' => 'foo',
    ),
));
```

The above route would match a path "/foo", and return the key "action" in the `RouteMatch`, with the
value "foo".

### Zend\\Mvc\\Router\\Http\\Method

The `Method` route is used to match the http method or 'verb' specified in the request (See RFC 2616
Sec. 5.1.1). It can optionally be configured to match against multiple methods by providing a
comma-separated list of method tokens.

```php
$route = Method::factory(array(
    'verb' => 'post,put',
    'defaults' => array(
        'controller' => 'Application\Controller\IndexController',
        'action' => 'form-submit',
    ),
));
```

The above route would match an http "POST" or "PUT" request and return a `RouteMatch` object
containing a key "action" with a value of "form-submit".

### Zend\\Mvc\\Router\\Http\\Part

A `Part` route allows crafting a tree of possible routes based on segments of the URI path. It
actually extends the `TreeRouteStack`.

`Part` routes are difficult to describe, so we'll simply provide a sample one here.

```php
$route = Part::factory(array(
    'route' => array(
        'type' => 'literal',
        'options' => array(
            'route' => '/',
            'defaults' => array(
                'controller' => 'Application\Controller\IndexController',
                'action' => 'index',
            ),
        ),
    ),
    'route_plugins' => $routePlugins,
    'may_terminate' => true,
    'child_routes' => array(
        'blog' => array(
            'type' => 'literal',
            'options' => array(
                'route' => '/blog',
                'defaults' => array(
                    'controller' => 'Application\Controller\BlogController',
                    'action' => 'index',
                ),
            ),
            'may_terminate' => true,
            'child_routes' => array(
                'rss' => array(
                    'type' => 'literal',
                    'options' => array(
                        'route' => '/rss',
                        'defaults' => array(
                            'action' => 'rss',
                        )
                    ),
                    'may_terminate' => true,
                    'child_routes' => array(
                        'subrss' => array(
                            'type' => 'literal',
                            'options' => array(
                                'route' => '/sub',
                                'defaults' => array(
                                    'action' => 'subrss',
                                ),
                            ),
                        ),
                    ),
                ),
            ),
        ),
        'forum' => array(
            'type' => 'literal',
            'options' => array(
                'route' => 'forum',
                'defaults' => array(
                    'controller' => 'Application\Controller\ForumController',
                    'action' => 'index',
                ),
            ),
        ),
    ),
));
```

The above would match the following:

- "/" would load the "Index" controller, "index" action.
- "/blog" would load the "Blog" controller, "index" action.
- "/blog/rss" would load the "Blog" controller, "rss" action.
- "/blog/rss/sub" would load the "Blog" controller, "subrss" action.
- "/forum" would load the "Forum" controller, "index" action.

You may use any route type as a child route of a `Part` route.

> ## Note
`Part` routes are not meant to be used directly. When you add definitions for `child_routes` to any
route type, that route will become a `Part` route. As already said, describing `Part` routes with
words is difficult, so hopefully the additional \[examples at the
end\](zend.mvc.routing.http-route-types.examples) will provide further insight.

> ## Note
In the above example, the `$routePlugins` is an instance of `Zend\Mvc\Router\RoutePluginManager`.
```php
$routePlugins = new Zend\Mvc\Router\RoutePluginManager();
$plugins = array(
'hostname' = 'Zend\Mvc\Router\Http\Hostname',
'literal'  = 'Zend\Mvc\Router\Http\Literal',
'part'     = 'Zend\Mvc\Router\Http\Part',
'regex'    = 'Zend\Mvc\Router\Http\Regex',
'scheme'   = 'Zend\Mvc\Router\Http\Scheme',
'segment'  = 'Zend\Mvc\Router\Http\Segment',
'wildcard' = 'Zend\Mvc\Router\Http\Wildcard',
'query'    = 'Zend\Mvc\Router\Http\Query',
'method'   = 'Zend\Mvc\Router\Http\Method',
);
foreach ($plugins as $name = $class) {
$routePlugins-setInvokableClass($name, $class);
}
```
When using `Zend\Mvc\Router\Http\TreeRouteStack`, the `RoutePluginManager` is set up by default, and
the developer does not need to worry about the autoloading of standard HTTP routes.

### Zend\\Mvc\\Router\\Http\\Regex

A `Regex` route utilizes a regular expression to match against the URI path. Any valid regular
expression is allowed; our recommendation is to use named captures for any values you want to return
in the `RouteMatch`.

Since regular expression routes are often complex, you must specify a "spec" or specification to use
when assembling URLs from regex routes. The spec is simply a string; replacements are identified
using "%keyname%" within the string, with the keys coming from either the captured values or named
parameters passed to the `assemble()` method.

Just like other routes, the `Regex` route can accept "defaults", parameters to include in the
`RouteMatch` when successfully matched.

```php
$route = Regex::factory(array(
    'regex' => '/blog/(?<id>[a-zA-Z0-9_-]+)(\.(?<format>(json|html|xml|rss)))?',
    'defaults' => array(
        'controller' => 'Application\Controller\BlogController',
        'action'     => 'view',
        'format'     => 'html',
    ),
    'spec' => '/blog/%id%.%format%',
));
```

The above would match "/blog/001-some-blog\_slug-here.html", and return four items in the
`RouteMatch`, an "id", the "controller", the "action", and the "format". When assembling a URL from
this route, the "id" and "format" values would be used to fill the specification.

### Zend\\Mvc\\Router\\Http\\Scheme

The `Scheme` route matches the URI scheme only, and must be an exact match. As such, this route,
like the `Literal` route, simply takes what you want to match and the "defaults", parameters to
return on a match.

```php
$route = Scheme::factory(array(
    'scheme' => 'https',
    'defaults' => array(
        'https' => true,
    ),
));
```

The above route would match the "https" scheme, and return the key "https" in the `RouteMatch` with
a boolean `true` value.

### Zend\\Mvc\\Router\\Http\\Segment

A `Segment` route allows matching any segment of a URI path. Segments are denoted using a colon,
followed by alphanumeric characters; if a segment is optional, it should be surrounded by brackets.
As an example, "/:foo\[/:bar\]" would match a "/" followed by text and assign it to the key "foo";
if any additional "/" characters are found, any text following the last one will be assigned to the
key "bar".

The separation between literal and named segments can be anything. For example, the above could be
done as "/:foo{-}\[-:bar\] as well. The {-} after the :foo parameter indicates a set of one or more
delimiters, after which matching of the parameter itself ends.

Each segment may have constraints associated with it. Each constraint should simply be a regular
expression expressing the conditions under which that segment should match.

Also, as you can in other routes, you may provide defaults to use; these are particularly useful
when using optional segments.

As a complex example:

```php
$route = Segment::factory(array(
    'route' => '/:controller[/:action]',
    'constraints' => array(
        'controller' => '[a-zA-Z][a-zA-Z0-9_-]+',
        'action'     => '[a-zA-Z][a-zA-Z0-9_-]+',
    ),
    'defaults' => array(
        'controller' => 'Application\Controller\IndexController',
        'action'     => 'index',
    ),
));
```

### Zend\\Mvc\\Router\\Http\\Query (Deprecated)

> ## Warning
#### Potential security issue
A misuse of this route part can lead to a potential security issue.

> ## Note
#### Deprecated
This route part is deprecated since you can now add query parameters without a query route.

The `Query` route part allows you to specify and capture query string parameters for a given route.

The intention of the `Query` part is that you do not instantiate it in its own right but to use it
as a child of another route part.

An example of its usage would be

```php
$route = Part::factory(array(
    'route' => array(
        'type'    => 'literal',
        'options' => array(
            'route'    => 'page',
            'defaults' => array(
            ),
        ),
    ),
    'may_terminate' => true,
    'route_plugins' => $routePlugins,
    'child_routes'  => array(
        'query' => array(
            'type' => 'Query',
            'options' => array(
                'defaults' => array(
                    'foo' => 'bar',
                ),
            ),
        ),
    ),
));
```

As you can see, it's pretty straight forward to specify the query part. This then allows you to
create query strings using the url view helper.

```php
$this->url(
    'page/query',
    array(
        'name'   => 'my-test-page',
        'format' => 'rss',
        'limit'  => 10,
    )
);
```

As you can see above, you must add "/query" to your route name in order to append a query string. If
you do not specify "/query" in the route name then no query string will be appended.

Our example "page" route has only one defined parameter of "name" ("/page\[/:name\]"), meaning that
the remaining parameters of "format" and "limit" will then be appended as a query string.

The output from our example should then be "/page/my-test-page?format=rss&limit=10"

### Zend\\Mvc\\Router\\Http\\Wildcard (Deprecated)

> ## Warning
#### Potential security issue
A misuse of this route type can lead to a potential security issue.

> ## Note
#### Deprecated
This route type is deprecated. Use the `Segment` route type.

The `Wildcard` route type matches all segments of a URI path, like in version 1 of Zend Framework.

## HTTP Routing Examples

Most of the routing definitions will be done in module configuration files, so the following
examples will show how to set up routes in config files.

**Simple example with two literal routes**

```php
return array(
    'router' => array(
        'routes' => array(
            // Literal route named "home"
            'home' => array(
                'type' => 'literal',
                'options' => array(
                    'route' => '/',
                    'defaults' => array(
                        'controller' => 'Application\Controller\IndexController',
                        'action' => 'index',
                    ),
                ),
            ),
            // Literal route named "contact"
            'contact' => array(
                'type' => 'literal',
                'options' => array(
                    'route' => 'contact',
                    'defaults' => array(
                        'controller' => 'Application\Controller\ContactController',
                        'action' => 'form',
                    ),
                ),
            ),
        ),
    ),
);
```

**A complex example with child routes**

```php
return array(
    'router' => array(
        'routes' => array(
            // Literal route named "home"
            'home' => array(
                'type' => 'literal',
                'options' => array(
                    'route' => '/',
                    'defaults' => array(
                        'controller' => 'Application\Controller\IndexController',
                        'action' => 'index',
                    ),
                ),
            ),
            // Literal route named "blog", with child routes
            'blog' => array(
                'type' => 'literal',
                'options' => array(
                    'route' => '/blog',
                    'defaults' => array(
                        'controller' => 'Application\Controller\BlogController',
                        'action' => 'index',
                    ),
                ),
                'may_terminate' => true,
                'child_routes' => array(
                    // Segment route for viewing one blog post
                    'post' => array(
                        'type' => 'segment',
                        'options' => array(
                            'route' => '/[:slug]',
                            'constraints' => array(
                                'slug' => '[a-zA-Z0-9_-]+',
                            ),
                            'defaults' => array(
                                'action' => 'view',
                            ),
                        ),
                    ),
                    // Literal route for viewing blog RSS feed
                    'rss' => array(
                        'type' => 'literal',
                        'options' => array(
                            'route' => '/rss',
                            'defaults' => array(
                                'action' => 'rss',
                            ),
                        ),
                    ),
                ),
            ),
        ),
    ),
);
```

When using child routes, naming of the routes follows the `parent/child` pattern, so to use the
child routes from the above example:

```php
echo $this->url('blog'); // gives "/blog"
echo $this->url('blog/post', array('slug' => 'my-post')); // gives "/blog/my-post"
echo $this->url('blog/rss'); // gives "/blog/rss"
```

**An example with multiple Hostnames and subdomains within a single application**

```php
return array(
    'router' => array(
        'routes' => array(
            'modules.zendframework.com' => array(
                'type' => 'Zend\Mvc\Router\Http\Hostname',
                'options' => array(
                    'route' => ':4th.[:3rd.]:2nd.:1st', // domain levels from right to left
                    'contraints' => array(
                        '4th' => 'modules',
                        '3rd' => '.*?', // optional 3rd level domain such as .ci, .dev or .test
                        '2nd' => 'zendframework',
                        '1st' => 'com',
                    ),
                    // Purposely omit default controller and action
                    // to let the child routes control the route match
                ),
                // child route controllers may span multiple modules as desired
                'child_routes' => array(
                    'index' => array(
                        'type' => 'Zend\Mvc\Router\Http\Literal',
                        'options' => array(
                            'route' => '/',
                            'defaults' => array(
                                'controller' => 'Module\Controller\Index',
                                'action' = > 'index',
                            ),
                        ),
                        'may_terminate' => true,
                    ),
                ),
            ),
            'packages.zendframework.com' => array(
                'type' => 'Zend\Mvc\Router\Http\Hostname',
                'options' => array(
                    'route' => ':4th.[:3rd.]:2nd.:1st', // domain levels from right to left
                    'contraints' => array(
                        '4th' => 'packages',
                        '3rd' => '.*?', // optional 3rd level domain such as .ci, .dev or .test
                        '2nd' => 'zendframework',
                        '1st' => 'com',
                    ),
                    // Purposely omit default controller and action
                    // to let the child routes control the route match
                ),
                // child route controllers may span multiple modules as desired
                'child_routes' => array(
                    'index' => array(
                        'type' => 'Zend\Mvc\Router\Http\Literal',
                        'options' => array(
                            'route' => '/',
                            'defaults' => array(
                                'controller' => 'Package\Controller\Index',
                                'action' = > 'index',
                            ),
                        ),
                        'may_terminate' => true,
                    ),
                ),
            ),
        ),
    ),
);
```

The above would match the following:

- `modules.zendframework.com` would dispatch the `Index` controller's `index` action of the `Module`
module.
- `modules.ci.zendframework.com` would dispatch the `Index` controller's `index` action of the
`Module` module.
- `packages.zendframework.com` would dispatch the `Index` controller's `index` action of the
`Package` module.
- `packages.dev.zendframework.com` would dispatch the `Index` controller's `index` action of the
`Package` module.

The `Url` controller plugin or view helper may be used to generate URLs following the above example:

```php
// reuse the route matched parameters to generate URLs
echo $this->url('modules.zendframework.com/index', array(), array(), true);
echo $this->url('packages.zendframework.com/index', array(), array(), true);
```

> ## Warning
When defining child routes pay attention that the `may_terminate` and `child_routes` definitions are
in same level as the `options` and `type` definitions. A common pitfall is to have those two
definitions nested in `options`, which will not result in the desired routes.

## Console Route Types

Zend Framework 2.0 also comes with routes for writing Console based applications, which is explained
in the \[Console routes and routing\](zend.console.routes) section.

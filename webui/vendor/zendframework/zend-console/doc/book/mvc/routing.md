# MVC Routing

zend-mvc provides integration with zend-console, routing command line arguments
to the appropriate action controller and action method that will handle the
request. Actions can perform any number of tasks prior to returning a result to
display in the console window.

When using zend-mvc, console routes are provided via configuration.

```php
// The following can sit inside of module/Application/config/module.config.php,
// any other module's configuration, or within files under config/autoload/:
return [
    'router' => [
        'routes' => [
            // HTTP routes are here
        ],
    ],

    'console' => [
        'router' => [
            'routes' => [
                // Console routes go here
            ],
        ],
    ],
];
```

Console routes exposed via zend-mvc will only be processed when the application
is run inside a console (terminal) window; they are not used for web (HTTP)
requests. It is possible to define only HTTP routes (web application only), only
console routes (console-only application), or a mixture of the two.

As an example of a single route:

```php
// inside config.console.router.routes:
'my-first-route' => [
    'type'    => 'simple',  // This is the default, and may be omitted; more on
                            // types below
    'options' => [
        'route'    => 'foo bar',
        'defaults' => [
            'controller' => Application\Controller\Index::class,
            'action'     => 'password',
        ],
    ],
],
```

We have created a `simple` console route with the name `my-first-route`. It
expects two parameters: `foo` and `bar`. If user puts these in a console,
`Application\Controller\IndexController::passwordAction()` action will be
invoked.

## Route types

In the last example of the previous section, we noted that configuration for a
given route can accept a `type` argument. This refers to the route type (which
maps to a class) to use when creating the route instance. zend-mvc defines
several types.

### Simple route

`Zend\Mvc\Router\Console\Simple` is a factory for zend-console's
`DefaultRouteMatcher`, and decorates it to work with the zend-mvc routing
system. See the section on [the default route matcher](../routes.md#the-default-route-matcher)
for details on route strings and available configuration to provide.

### Catchall route

This special route will catch all console requests, regardless of the parameters provided.

```php
'default-route' => [
    'type'     => 'catchall',
    'options' => [
        'route'    => '',
        'defaults' => [
            'controller' => 'Application\Controller\Index',
            'action'     => 'consoledefault',
        ],
    ],
],
```

This route type is rarely used; one possible use case is to register it as the
last console route, in order to display usage information. Before you do so,
read about the [preferred way of displaying console usage
information](modules.md); following recommendations will guarantee proper
interoperation with other modules in your application.

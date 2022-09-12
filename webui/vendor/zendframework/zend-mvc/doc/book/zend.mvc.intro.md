# Introduction to the MVC Layer

`Zend\Mvc` is a brand new MVC implementation designed from the ground up for Zend Framework 2,
focusing on performance and flexibility.

The MVC layer is built on top of the following components:

- `Zend\ServiceManager` - Zend Framework provides a set of default service definitions set up at
`Zend\Mvc\Service`. The `ServiceManager` creates and configures your application instance and
workflow.
- `Zend\EventManager` - The MVC is event driven. This component is used everywhere from initial
bootstrapping of the application, through returning response and request calls, to setting and
retrieving routes and matched routes, as well as render views.
- `Zend\Http` - specifically the request and response objects, used within:
- `Zend\Stdlib\DispatchableInterface`. All "controllers" are simply dispatchable objects.

Within the MVC layer, several sub-components are exposed:

- `Zend\Mvc\Router` contains classes pertaining to routing a request. In other words, it matches the
request to its respective controller (or dispatchable).
- `Zend\Http\PhpEnvironment` provides a set of decorators for the HTTP `Request` and `Response`
objects that ensure the request is injected with the current environment (including query
parameters, POST parameters, HTTP headers, etc.)
- `Zend\Mvc\Controller`, a set of abstract "controller" classes with basic responsibilities such as
event wiring, action dispatching, etc.
- `Zend\Mvc\Service` provides a set of `ServiceManager` factories and definitions for the default
application workflow.
- `Zend\Mvc\View` provides default wiring for renderer selection, view script resolution, helper
registration, and more; additionally, it provides a number of listeners that tie into the MVC
workflow, providing features such as automated template name resolution, automated view model
creation and injection, and more.

The gateway to the MVC is the
[Zend\\Mvc\\Application](https://github.com/zendframework/zf2/blob/master/library/Zend/Mvc/Application.php)
object (referred to as `Application` henceforth). Its primary responsibilities are to **bootstrap**
resources, **route** the request, and to retrieve and **dispatch** the controller matched during
routing. Once accomplished, it will **render** the view, and **finish** the request, returning and
sending the response.

## Basic Application Structure

The basic application structure follows:

    application_root/
        config/
            application.config.php
            autoload/
                global.php
                local.php
                // etc.
        data/
        module/
        vendor/
        public/
            .htaccess
            index.php
        init_autoloader.php

The `public/index.php` marshalls all user requests to your website, retrieving an array of
configuration located in `config/application.config.php`. On return, it `run()`s the `Application`,
processing the request and returning a response to the user.

The `config` directory as described above contains configuration used by the `Zend\ModuleManager` to
load modules and merge configuration (e.g., database configuration and credentials); we will detail
this more later.

The `vendor` sub-directory should contain any third-party modules or libraries on which your
application depends. This might include Zend Framework, custom libraries from your organization, or
other third-party libraries from other projects. Libraries and modules placed in the `vendor`
sub-directory should not be modified from their original, distributed state.

Finally, the `module` directory will contain one or more modules delivering your application's
functionality.

Let's now turn to modules, as they are the basic units of a web application.

## Basic Module Structure

A module may contain anything: PHP code, including MVC functionality; library code; view scripts;
and/or or public assets such as images, CSS, and JavaScript. The only requirement -- and even this
is optional -- is that a module acts as a PHP namespace and that it contains a `Module.php` class
under that namespace. This class is eventually consumed by `Zend\ModuleManager` to perform a number
of tasks.

The recommended module structure follows:

    module_root<named-after-module-namespace>/
        Module.php
        autoload_classmap.php
        autoload_function.php
        autoload_register.php
        config/
            module.config.php
        public/
            images/
            css/
            js/
        src/
            <module_namespace>/
                <code files>
        test/
            phpunit.xml
            bootstrap.php
            <module_namespace>/
                <test code files>
        view/
            <dir-named-after-module-namespace>/
                <dir-named-after-a-controller>/
                    <.phtml files>

Since a module acts as a namespace, the module root directory should be that namespace. This
namespace could also include a vendor prefix of sorts. As an example a module centered around "User"
functionality delivered by Zend might be named "ZendUser", and this is also what the module root
directory will be named.

The `Module.php` file directly under the module root directory will be in the module namespace shown
below.

```php
namespace ZendUser;

class Module
{
}
```

When an `init()` method is defined, this method will be triggered by a `Zend\ModuleManager` listener
when it loads the module class, and passed an instance of the manager by default. This allows you to
perform tasks such as setting up module-specific event listeners. But be cautious, the `init()`
method is called for **every** module on **every** page request and should **only** be used for
performing **lightweight** tasks such as registering event listeners. Similarly, an `onBootstrap()`
method (which accepts an `MvcEvent` instance) may be defined; it is also triggered for every page
request, and should be used for lightweight tasks as well.

The three `autoload_*.php` files are not required, but recommended. They provide the following:

The point of these three files is to provide reasonable default mechanisms for autoloading the
classes contained in the module, thus providing a trivial way to consume the module without
requiring `Zend\ModuleManager` (e.g., for use outside a ZF2 application).

The `config` directory should contain any module-specific configuration. These files may be in any
format `Zend\Config` supports. We recommend naming the main configuration "module.format", and for
PHP-based configuration, "module.config.php". Typically, you will create configuration for the
router as well as for the dependency injector.

The `src` directory should be a [PSR-0 compliant directory
structure](https://github.com/php-fig/fig-standards/blob/master/accepted/PSR-0.md) with your
module's source code. Typically, you should at least have one sub-directory named after your module
namespace; however, you can ship code from multiple namespaces if desired.

The `test` directory should contain your unit tests. Typically, these are written using
[PHPUnit](http://phpunit.de), and contain artifacts related to its configuration (e.g.,
`phpunit.xml`, `bootstrap.php`).

The `public` directory can be used for assets that you may want to expose in your application's
document root. These might include images, CSS files, JavaScript files, etc. How these are exposed
is left to the developer.

The `view` directory contains view scripts related to your controllers.

## Bootstrapping an Application

The `Application` has six basic dependencies.

- **configuration**, usually an array or object implementing `Traversable`.
- **ServiceManager** instance.
- **EventManager** instance, which, by default, is pulled from the `ServiceManager`, by the service
name "EventManager".
- **ModuleManager** instance, which, by default, is pulled from the `ServiceManager`, by the service
name "ModuleManager".
- **Request** instance, which, by default, is pulled from the `ServiceManager`, by the service name
"Request".
- **Response** instance, which, by default, is pulled from the `ServiceManager`, by the service name
"Response".

These may be satisfied at instantiation:

```php
use Zend\EventManager\EventManager;
use Zend\Http\PhpEnvironment;
use Zend\ModuleManager\ModuleManager;
use Zend\Mvc\Application;
use Zend\ServiceManager\ServiceManager;

$config = include 'config/application.config.php';

$serviceManager = new ServiceManager();
$serviceManager->setService('EventManager', new EventManager());
$serviceManager->setService('ModuleManager', new ModuleManager($config));
$serviceManager->setService('Request', new PhpEnvironment\Request());
$serviceManager->setService('Response', new PhpEnvironment\Response());

$application = new Application($config, $serviceManager);
```

Once you've done this, there are two additional actions you can take. The first is to "bootstrap"
the application. In the default implementation, this does the following:

- Attaches the default route listener (`Zend\Mvc\RouteListener`).
- Attaches the default dispatch listener (`Zend\Mvc\DispatchListener`).
- Attaches the `ViewManager` listener (`Zend\Mvc\View\ViewManager`).
- Creates the `MvcEvent`, and injects it with the application, request, and response; it also
retrieves the router (`Zend\Mvc\Router\Http\TreeRouteStack`) at this time and attaches it to the
event.
- Triggers the "bootstrap" event.

If you do not want these actions, or want to provide alternatives, you can do so by extending the
`Application` class and/or simply coding what actions you want to occur.

The second action you can take with the configured `Application` is to `run()` it. Calling this
method simply does the following: it triggers the "route" event, followed by the "dispatch" event,
and, depending on execution, the "render" event; when done, it triggers the "finish" event, and then
returns the response instance. If an error occurs during either the "route" or "dispatch" event, a
"dispatch.error" event is triggered as well.

This is a lot to remember in order to bootstrap the application; in fact, we haven't covered all the
services available by default yet. You can greatly simplify things by using the default
`ServiceManager` configuration shipped with the MVC.

```php
use Zend\Loader\AutoloaderFactory;
use Zend\Mvc\Service\ServiceManagerConfig;
use Zend\ServiceManager\ServiceManager;

// setup autoloader
AutoloaderFactory::factory();

// get application stack configuration
$configuration = include 'config/application.config.php';

// setup service manager
$serviceManager = new ServiceManager(new ServiceManagerConfig());
$serviceManager->setService('ApplicationConfig', $configuration);

// load modules -- which will provide services, configuration, and more
$serviceManager->get('ModuleManager')->loadModules();

// bootstrap and run application
$application = $serviceManager->get('Application');
$application->bootstrap();
$application->run();
```

You can make this even simpler by using the `init()` method of the `Application`. This is a static
method for quick and easy initialization of the Application.

```php
use Zend\Loader\AutoloaderFactory;
use Zend\Mvc\Application;
use Zend\Mvc\Service\ServiceManagerConfig;
use Zend\ServiceManager\ServiceManager;

// setup autoloader
AutoloaderFactory::factory();

// get application stack configuration
$configuration = include 'config/application.config.php';

// The init() method does something very similar with the previous example.
Application::init($configuration)->run();
```

The `init()` method will basically do the following:  
- Grabs the application configuration and pulls from the `service_manager` key, creating a
`ServiceManager`  
    instance with it and with the default services shipped with `Zend\Mvc`;

- Create a service named `ApplicationConfig` with the application configuration array;
- Grabs the `ModuleManager` service and load the modules;
- `bootstrap()`s the `Application` and returns its instance;

> ## Note
If you use the `init()` method, you cannot specify a service with the name of 'ApplicationConfig' in
your service manager config. This name is reserved to hold the array from application.config.php.
The following services can only be overridden from application.config.php:  
- `ModuleManager`
- `SharedEventManager`
- `EventManager` & `Zend\EventManager\EventManagerInterface`
All other services are configured after module loading, thus can be overridden by modules.

You'll note that you have a great amount of control over the workflow. Using the `ServiceManager`,
you have fine-grained control over what services are available, how they are instantiated, and what
dependencies are injected into them. Using the `EventManager`'s priority system, you can intercept
any of the application events ("bootstrap", "route", "dispatch", "dispatch.error", "render", and
"finish") anywhere during execution, allowing you to craft your own application workflows as needed.

## Bootstrapping a Modular Application

While the previous approach largely works, where does the configuration come from? When we create a
modular application, the assumption will be that it's from the modules themselves. How do we get
that information and aggregate it, then?

The answer is via `Zend\ModuleManager\ModuleManager`. This component allows you to specify where
modules exist. Then, it will locate each module and initialize it. Module classes can tie into
various listeners on the `ModuleManager` in order to provide configuration, services, listeners, and
more to the application. Sounds complicated? It's not.

### Configuring the Module Manager

The first step is configuring the module manager. Simply inform the module manager which modules to
load, and potentially provide configuration for the module listeners.

Remember the `application.config.php` from earlier? We're going to provide some configuration.

```php
<?php
// config/application.config.php
return array(
    'modules' => array(
        /* ... */
    ),
    'module_listener_options' => array(
        'module_paths' => array(
            './module',
            './vendor',
        ),
    ),
);
```

As we add modules to the system, we'll add items to the `modules` array.

Each `Module` class that has configuration it wants the `Application` to know about should define a
`getConfig()` method. That method should return an array or `Traversable` object such as
`Zend\Config\Config`. As an example:

```php
namespace ZendUser;

class Module
{
    public function getConfig()
    {
        return include __DIR__ . '/config/module.config.php'
    }
}
```

There are a number of other methods you can define for tasks ranging from providing autoloader
configuration, to providing services to the `ServiceManager`, to listening to the bootstrap event.
The ModuleManager
documentation &lt;zend.module-manager.intro&gt; goes into more detail on these.

## Conclusion

The ZF2 MVC layer is incredibly flexible, offering an opt-in, easy to create modular infrastructure,
as well as the ability to craft your own application workflows via the `ServiceManager` and
`EventManager`. The `ModuleManager` is a lightweight and simple approach to enforcing a modular
architecture that encourages clean separation of concerns and code re-use.

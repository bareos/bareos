# Quick Start

Now that you have basic knowledge of applications, modules, and how they are each structured, we'll
show you the easy way to get started.

## Install the Zend Skeleton Application

The easiest way to get started is to grab the sample application and module repositories. This can
be done in the following ways.

### Using Composer

Simply clone the `ZendSkeletonApplication` repository:

```php
prompt> git clone git://github.com/zendframework/ZendSkeletonApplication.git my-application
```

Then run [Composer](http://getcomposer.org/)'s `install` command to install the ZF library and any
other configured dependencies:

```php
prompt> php ./composer.phar install
```

### Using Git

Simply clone the `ZendSkeletonApplication` repository, using the `--recursive` option, which will
also grab ZF.

```php
prompt> git clone --recursive git://github.com/zendframework/ZendSkeletonApplication.git
my-application
```

### Manual Installation

- Download a tarball of the `ZendSkeletonApplication` repository:
- Zip: <https://github.com/zendframework/ZendSkeletonApplication/zipball/master>
- Tarball: <https://github.com/zendframework/ZendSkeletonApplication/tarball/master>
- Deflate the archive you selected and rename the parent directory according to your project needs;
we use "my-application" throughout this document.
- Install Zend Framework, and either have its library on your PHP `include_path`, symlink the
library into your project's "library", or install it directly into your application using Pyrus.

## Create a New Module

By default, one module is provided with the `ZendSkeletonApplication`, named "Application". It
simply provides a controller to handle the "home" page of the application, the layout template, and
templates for 404 and error pages.

Typically, you will not need to touch this other than to provide an alternate entry page for your
site and/or alternate error page.

Additional functionality will be provided by creating new modules.

To get you started with modules, we recommend using the `ZendSkeletonModule` as a base. Download it
from here:

- Zip: <https://github.com/zendframework/ZendSkeletonModule/zipball/master>
- Tarball: <https://github.com/zendframework/ZendSkeletonModule/tarball/master>

Deflate the package, and rename the directory "ZendSkeletonModule" to reflect the name of the new
module you want to create; when done, move the module into your new project's `module/` directory.

At this point, it's time to create some functionality.

## Update the Module Class

Let's update the module class. We'll want to make sure the namespace is correct, configuration is
enabled and returned, and that we setup autoloading on initialization. Since we're actively working
on this module, the class list will be in flux, we probably want to be pretty lenient in our
autoloading approach, so let's keep it flexible by using the `StandardAutoloader`. Let's begin.

First, let's have `autoload_classmap.php` return an empty array:

```php
<?php
// autoload_classmap.php
return array();
```

We'll also edit our `config/module.config.php` file to read as follows:

```php
return array(
    'view_manager' => array(
        'template_path_stack' => array(
            '<module-name>' => __DIR__ . '/../view'
        ),
    ),
);
```

Fill in "module-name" with a lowercased, dash-separated version of your module name -- e.g.,
"ZendUser" would become "zend-user".

Next, edit the `Module.php` file to read as follows:

```php
namespace <your module name here>;

use Zend\ModuleManager\Feature\AutoloaderProviderInterface;
use Zend\ModuleManager\Feature\ConfigProviderInterface;

class Module implements AutoloaderProviderInterface, ConfigProviderInterface
{
    public function getAutoloaderConfig()
    {
        return array(
            'Zend\Loader\ClassMapAutoloader' => array(
                __DIR__ . '/autoload_classmap.php',
            ),
            'Zend\Loader\StandardAutoloader' => array(
                'namespaces' => array(
                    __NAMESPACE__ => __DIR__ . '/src/' . __NAMESPACE__,
                ),
            ),
        );
    }

    public function getConfig()
    {
        return include __DIR__ . '/config/module.config.php';
    }
}
```

At this point, you now have your module configured properly. Let's create a controller!

## Create a Controller

Controllers are simply objects that implement `Zend\Stdlib\DispatchableInterface`. This means they
need to implement a `dispatch()` method that takes minimally a `Request` object as an argument.

In practice, though, this would mean writing logic to branch based on matched routing within every
controller. As such, we've created two base controller classes for you to start with:

- `Zend\Mvc\Controller\AbstractActionController` allows routes to match an "action". When matched, a
method named after the action will be called by the controller. As an example, if you had a route
that returned "foo" for the "action" key, the "fooAction" method would be invoked.
- `Zend\Mvc\Controller\AbstractRestfulController` introspects the `Request` to determine what HTTP
method was used, and calls a method according to that.
- `GET` will call either the `getList()` method, or, if an "id" was matched during routing, the
`get()` method (with that identifer value).
- `POST` will call the `create()` method, passing in the `$_POST` values.
- `PUT` expects an "id" to be matched during routing, and will call the `update()` method, passing
in the identifier, and any data found in the raw post body.
- `DELETE` expects an "id" to be matched during routing, and will call the `delete()` method.

To get started, we'll simply create a "hello world"-style controller, with a single action. First,
create the directory `src/<module name>/Controller`, and then create the file `HelloController.php`
inside it. Edit it in your favorite text editor or IDE, and insert the following contents:

```php
<?php
namespace <module name>\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class HelloController extends AbstractActionController
{
    public function worldAction()
    {
        $message = $this->params()->fromQuery('message', 'foo');
        return new ViewModel(array('message' => $message));
    }
}
```

So, what are we doing here?

- We're creating an action controller.
- We're defining an action, "world".
- We're pulling a message from the query parameters (yes, this is a superbly bad idea in production!
Always sanitize your inputs!).
- We're returning a ViewModel with an array of values to be processed later.

We return a `ViewModel`. The view layer will use this when rendering the view, pulling variables and
the template name from it. By default, you can omit the template name, and it will resolve to
"lowercase-controller-name/lowercase-action-name". However, you can override this to specify
something different by calling `setTemplate()` on the `ViewModel` instance. Typically, templates
will resolve to files with a ".phtml" suffix in your module's `view` directory.

So, with that in mind, let's create a view script.

## Create a View Script

Create the directory `view/<module-name>/hello`. Inside that directory, create a file named
`world.phtml`. Inside that, paste in the following:

```php
<h1>Greetings!</h1>

<p>You said "<?php echo $this->escapeHtml($message) ?>".</p>
```

That's it. Save the file.

> ## Note
What is the method `escapeHtml()`? It's actually a \[view helper\](zend.view.helpers), and it's
designed to help mitigate *XSS* attacks. Never trust user input; if you are at all uncertain about
the source of a given variable in your view script, escape it using one of the \[provided escape
view helper\](zend.view.helpers) depending on the type of data you have.

## View scripts for module names with subnamespaces

As per PSR-0, module should be named following this rule: `\<Vendor Name>\(<Namespace>\)*` Default
controller class to template mapping does not work very well with those: it will remove
subnamespace.

To address that issue new mapping was introduced since 2.3.0. To maintain backwards compatibility
that mapping is not enabled by default. To enable it, you need to add your module namespace to
whitelist in your module config:

```php
'view_manager' => array(
    // Controller namespace to template map
    // or whitelisting for controller FQCN to template mapping
    'controller_map' => array(
        '<Module\Name>' => true,
    ),
),
```

Now, create the directory `view/<module>/<name>/hello`. Inside that directory, create a file named
`world.phtml`. Inside that, paste in the following:

```php
<h1>Greetings!</h1>

<p>You said "<?php echo $this->escapeHtml($message) ?>".</p>
```

## Create a Route

Now that we have a controller and a view script, we need to create a route to it.

> ## Note
`ZendSkeletonApplication` ships with a "default route" that will likely get you to this action. That
route basically expects "/{module}/{controller}/{action}", which allows you to specify this:
"/zend-user/hello/world". We're going to create a route here mainly for illustration purposes, as
creating explicit routes is a recommended practice. The application will look for a
`Zend\Mvc\Router\RouteStack` instance to setup routing. The default generated router is a
`Zend\Mvc\Router\Http\TreeRouteStack`.
To use the "default route" functionality, you will need to have the following route definition in
your module. Replace &lt;module-name&gt; with the name of your module.
```php
// module.config.php
return array(
'<module-name' = array(
'type'    = 'Literal',
'options' = array(
'route'    = '/<module-name',
'defaults' = array(
'controller'    = '<module-namespace\Controller\Index',
'action'        = 'index',
),
),
'may_terminate' = true,
'child_routes' = array(
'default' = array(
'type'    = 'Segment',
'options' = array(
'route'    = '/[:controller[/:action]]',
'constraints' = array(
'controller' = '[a-zA-Z][a-zA-Z0-9_-]*',
'action'     = '[a-zA-Z][a-zA-Z0-9_-]*',
),
'defaults' = array(
),
),
),
),
),
// ... other configuration ...
);
```

Additionally, we need to tell the application we have a controller:

> ```php
// module.config.php
return array(
'controllers' = array(
'invokables' = array(
'<module-namespace\Controller\Index' = '<module-namespace\Controller\IndexController',
// Do similar for each other controller in your module
),
),
// ... other configuration ...
);
```

> ## Note
We inform the application about controllers we expect to have in the application. This is to prevent
somebody requesting any service the `ServiceManager` knows about in an attempt to break the
application. The dispatcher uses a special, scoped container that will only pull controllers that
are specifically registered with it, either as invokable classes or via factories.

Open your `config/module.config.php` file, and modify it to add to the "routes" and "controller"
parameters so it reads as follows:

```php
return array(
    'router' => array(
        'routes' => array(
            '<module name>-hello-world' => array(
                'type'    => 'Literal',
                    'options' => array(
                    'route' => '/hello/world',
                    'defaults' => array(
                        'controller' => '<module name>\Controller\Hello',
                        'action'     => 'world',
                    ),
                ),
            ),
        ),
    ),
    'controllers' => array(
        'invokables' => array(
            '<module namespace>\Controller\Hello' => '<module
namespace>\Controller\HelloController',
        ),
    ),
    // ... other configuration ...
);
```

## Tell the Application About our Module

One problem: we haven't told our application about our new module!

By default, modules are not parsed unless we tell the module manager about them. As such, we need to
notify the application about them.

Remember the `config/application.config.php` file? Let's modify it to add our new module. Once done,
it should read as follows:

```php
<?php
return array(
    'modules' => array(
        'Application',
        '<module namespace>',
    ),
    'module_listener_options' => array(
        'module_paths' => array(
            './module',
            './vendor',
        ),
    ),
);
```

Replace `<module namespace>` with the namespace of your module.

## Test it Out!

Now we can test things out! Create a new vhost pointing its document root to the `public` directory
of your application, and fire it up in a browser. You should see the default homepage template of
ZendSkeletonApplication.

Now alter the location in your URL to append the path "/hello/world", and load the page. You should
now get the following content:

```php
<h1>Greetings!</h1>

<p>You said "foo".</p>
```

Now alter the location to append "?message=bar" and load the page. You should now get:

```php
<h1>Greetings!</h1>

<p>You said "bar".</p>
```

Congratulations! You've created your first ZF2 MVC module!

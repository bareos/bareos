# Default Services

The default and recommended way to write Zend Framework applications uses a set of services defined
in the `Zend\Mvc\Service` namespace. This chapter details what each of those services are, the
classes they represent, and the configuration options available.

Many of the services are provided by other components, and the factories and abstract factories
themselves are defined in the individual components. We will cover those factories in this chapter,
however, as usage is generally the same between each.

## Theory of Operation

To allow easy configuration of all the different parts of the MVC system, a somewhat complex set of
services and their factories has been created. We'll try to give a simplified explanation of the
process.

When a `Zend\Mvc\Application` is created, a `Zend\ServiceManager\ServiceManager` object is created
and configured via `Zend\Mvc\Service\ServiceManagerConfig`. The `ServiceManagerConfig` gets the
configuration from `application.config.php` (or some other application configuration you passed to
the `Application` when creating it). From all the service and factories provided in the
`Zend\Mvc\Service` namespace, `ServiceManagerConfig` is responsible of configuring only three:
`SharedEventManager`, `EventManager`, and `ModuleManager`.

After this, the `Application` calls for the `ModuleManager`. At this point, the `ModuleManager`
further configures the `ServiceManager` with services and factories provided in
`Zend\Mvc\Service\ServiceListenerFactory`. This approach allows us to keep the main application
configuration concise, and to give the developer the power to configure different parts of the MVC
system from within the modules, overriding any default configuration in these MVC services.

## ServiceManager

As a quick review, the following service types may be configured:

- **Invokable services**, which map a service name to a class that has no constructor or a
constructor that accepts no arguments.
- **Factories**, which map a service name to a factory which will create and return an object. A
factory receives the service manager as an argument, and may be any PHP callable, or a class or
object that implements `Zend\ServiceManager\FactoryInterface`.
- **Abstract factories**, which are factories that can create any number of named services that
share the same instantiation pattern; examples include database adapters, cache adapters, loggers,
etc. The factory receives the service manager as an argument, the resolved service name, and the
requested service name; it **must** be a class or object implementing
`Zend\ServiceManager\AbstractFactoryInterface`. See the section on abstract factories
    &lt;zend.mvc.services.abstract-factories&gt; for configuration information.
- **Aliases**, which alias one service name to another. Aliases can also reference other aliases.
- **Initializers**, which receive the newly created instance and the service manager, and which can
be used to perform additional initialization tasks. The most common use case is to test the instance
against specific "Aware" interfaces, and, if matching, inject them with the appropriate service.
- **Plugin managers**, which are specialized service managers used to manage objects that are of a
related type, such as view helpers, controller plugins, controllers, etc. Plugin managers accept
configuration just like service managers, and as such can compose invokable services, factories,
abstract factories, aliases, and initializers. They are also `ServiceLocatorAware`, and will be
injected with the application service manager instance, giving factories and abstract factories
access to application-level services when needed. See the heading Plugin managers
    &lt;zend.mvc.services.plugin-managers&gt; for a list of available plugin managers.

The application service manager is referenced directly during bootstrapping, and has the following
services configured out of the box.

- **Invokable services**
- `DispatchListener`, mapping to `Zend\Mvc\DispatchListener`.
- `RouteListener`, mapping to `Zend\Mvc\RouteListener`.
- `SendResponseListener`, mapping to `Zend\Mvc\SendResponseListener`.
- `SharedEventManager`, mapping to `Zend\EventManager\SharedEventManager`.
- **Factories**
- `Application`, mapping to `Zend\Mvc\Service\ApplicationFactory`.
- `Config`, mapping to `Zend\Mvc\Service\ConfigFactory`. Internally, this pulls the `ModuleManager`
service, and calls its `loadModules()` method, and retrieves the merged configuration from the
module event. As such, this service contains the entire, merged application configuration.
- `ControllerManager`, mapping to `Zend\Mvc\Service\ControllerLoaderFactory`. This creates an
instance of `Zend\Mvc\Controller\ControllerManager`, passing the service manager instance.

        Additionally, it uses the `DiStrictAbstractServiceFactory` service -- effectively allowing
you to fall back to DI in order to retrieve your controllers. If you want to use `Zend\Di` to
retrieve your controllers, you must white-list them in your DI configuration under the
`allowed_controllers` key (otherwise, they will just be ignored).

        The `ControllerManager` will add an initializer that will do the following:

        > -   If the controller implements the `Zend\ServiceManager\ServiceLocatorAwareInterface`
interface, an instance of the `ServiceManager` will be injected into it.
        > -   If the controller implements the `Zend\EventManager\EventManagerAwareInterface`
interface, an instance of the `EventManager` will be injected into it.
        > -   Finally, an initializer will inject it with the `ControllerPluginManager` service, as
long as the `setPluginManager` method is implemented.

- `ControllerPluginManager`, mapping to `Zend\Mvc\Service\ControllerPluginManagerFactory`. This
instantiates the `Zend\Mvc\Controller\PluginManager` instance, passing it the service manager
instance. It also uses the `DiAbstractServiceFactory` service -- effectively allowing you to fall
back to DI in order to retrieve your controller plugins
        &lt;zend.mvc.controller-plugins&gt;.

        It registers a set of default controller plugins, and contains an initializer for injecting
plugins with the current controller.

- `ConsoleAdapter`, mapping to `Zend\Mvc\Service\ConsoleAdapterFactory`. This grabs the `Config`
service, pulls from the `console` key, and do the following:
- If the `adapter` subkey is present, it is used to get the adapter instance, otherwise,
`Zend\Console\Console::detectBestAdapter()` will be called to configure an adapter instance.
- If the `charset` subkey is present, the is used to set the adapter charset.
- `ConsoleRouter`, mapping to `Zend\Mvc\Service\RouterFactory`. This grabs the `Config` service, and
pulls from the `console` key and `router` subkey, configuring a
`Zend\Mvc\Router\Console\SimpleRouteStack` instance.
- `ConsoleViewManager`, mapping to `Zend\Mvc\Service\ConsoleViewManagerFactory`. This creates and
returns an instance of `Zend\Mvc\View\Console\ViewManager`, which in turn registers and initializes
a number of console-specific view services.
- `DependencyInjector`, mapping to `Zend\Mvc\Service\DiFactory`. This pulls the `Config` service,
and looks for a "di" key; if found, that value is used to configure a new `Zend\Di\Di` instance.
- `DiAbstractServiceFactory`, mapping to `Zend\Mvc\Service\DiAbstractServiceFactoryFactory`. This
creates an instance of `Zend\ServiceManager\Di\DiAbstractServiceFactory` injecting the `Di` service
instance. That instance is attached to the service manager as an abstract factory -- effectively
enabling DI as a fallback for providing services.
- `DiServiceInitializer`, mapping to `Zend\Mvc\Service\DiServiceInitializerFactory`. This creates an
instance of `Zend\ServiceManager\Di\DiServiceInitializer` injecting the `Di` service and the service
manager itself.
- `DiStrictAbstractServiceFactory`, mapping to
`Zend\Mvc\Service\DiStrictAbstractServiceFactoryFactory`. This creates an instance of
`Zend\Mvc\Service\DiStrictAbstractServiceFactoryFactory` injecting the `Di` service instance.
- `EventManager`, mapping to `Zend\Mvc\Service\EventManagerFactory`. This factory returns a new
instance of `Zend\EventManager\EventManager` on each request. This service is not shared by default,
allowing the ability to have an `EventManager` per service, with a shared `SharedEventManager`
injected in each.
- `FilterManager`, mapping to `Zend\Mvc\Service\FilterManagerFactory`. This instantiates the
`Zend\Filter\FilterPluginManager` instance, passing it the service manager instance -- this is used
to manage filters for the \[filter chains\](zend.filter.filter\_chains). It also uses the
`DiAbstractServiceFactory` service -- effectively allowing you to fall back to DI in order to
retrieve filters.
- `FormElementManager`, mapping to `Zend\Mvc\Service\FormElementManagerFactory`. This instantiates
the `Zend\Form\FormElementManager` instance, passing it the service manager instance -- this is used
to manage \[form elements\](zend.form.elements.intro). It also uses the `DiAbstractServiceFactory`
service -- effectively allowing you to fall back to DI in order to retrieve form elements.
- `HttpRouter`, mapping to `Zend\Mvc\Service\RouterFactory`. This grabs the `Config` service, and
pulls from the `router` key, configuring a `Zend\Mvc\Router\Http\TreeRouteStack` instance.
- `HttpViewManager`, mapping to `Zend\Mvc\Service\HttpViewManagerFactory`. This creates and returns
an instance of `Zend\Mvc\View\Http\ViewManager`, which in turn registers and initializes a number of
HTTP-specific view services.
- `HydratorManager`, mapping to `Zend\Mvc\Service\HydratorManagerFactory`. This creates and returns
an instance of `Zend\Stdlib\Hydrator\HydratorPluginManager`, which can be used to manage and persist
hydrator instances.
- `InputFilterManager`, mapping to `Zend\Mvc\Service\InputFilterManagerFactory`. This creates and
returns an instance of `Zend\InputFilter\InputFilterPluginManager`, which can be used to manage and
persist input filter instances.
- `ModuleManager`, mapping to `Zend\Mvc\Service\ModuleManagerFactory`.

        This is perhaps the most complex factory in the MVC stack. It expects that an
`ApplicationConfig` service has been injected, with keys for `module_listener_options` and
`modules`; see the quick start for samples.

        It instantiates an instance of `Zend\ModuleManager\Listener\DefaultListenerAggregate`, using
the "module\_listener\_options" retrieved. Checks if a service with the name `ServiceListener`
exists, otherwise it sets a factory with that name mapping to
`Zend\Mvc\Service\ServiceListenerFactory`. A bunch of service listeners will be added to the
`ServiceListener`, like listeners for the `getServiceConfig`, `getControllerConfig`,
`getControllerPluginConfig`, `getViewHelperConfig` module methods.

        Next, it retrieves the `EventManager` service, and attaches the above listeners.

        It instantiates a `Zend\ModuleManager\ModuleEvent` instance, setting the "ServiceManager"
parameter to the service manager object.

        Finally, it instantiates a `Zend\ModuleManager\ModuleManager` instance, and injects the
`EventManager` and `ModuleEvent`.

- `MvcTranslator`, mapping to `Zend\Mvc\Service\TranslatorServiceFactory`, and returning an instance
of `Zend\Mvc\I18n\Translator`, which extends `Zend\I18n\Translator\Translator` and implements
`Zend\Validator\Translator\TranslatorInterface`, allowing the instance to be used anywhere a
translator may be required in the framework.
- `PaginatorPluginManager`, mapping to `Zend\Mvc\Service\PaginatorPluginManagerFactory`. This
instantiates the `Zend\Paginator\AdapterPluginManager` instance, passing it the service manager
instance -- this is used to manage paginator adapters
        &lt;zend.paginator.usage.paginating.adapters&gt;. It also uses the
`DiAbstractServiceFactory` service -- effectively allowing you to fall back to DI in order to
retrieve paginator adapters.
- `Request`, mapping to `Zend\Mvc\Service\RequestFactory`. The factory is used to create and return
a request instance, according to the current environment. If the current environment is `cli`, it
will create a `Zend\Console\Request`, or a `Zend\Http\PhpEnvironment\Request` if the current
environment is HTTP.
- `Response`, mapping to `Zend\Mvc\Service\ResponseFactory`. The factory is used to create and
return a response instance, according to the current environment. If the current environment is
`cli`, it will create a `Zend\Console\Response`, or a `Zend\Http\PhpEnvironment\Response` if the
current environment is HTTP.
- `Router`, mapping to `Zend\Mvc\Service\RouterFactory`. If in a console enviroment, this will
behave the same way as the `ConsoleRouter` service, if not, it will behave the same way as
`HttpRouter` service.
- `RoutePluginManager`, mapping to `Zend\Mvc\Service\RoutePluginManagerFactory`. This instantiates
the `Zend\Mvc\Router\RoutePluginManager` instance, passing it the service manager instance -- this
is used to manage \[route types\](zend.mvc.routing.http-route-types). It also uses the
`DiAbstractServiceFactory` service -- effectively allowing you to fall back to DI in order to
retrieve route types.
- `SerializerAdapterManager`, mapping to `Zend\Mvc\Service\SerializerAdapterPluginManagerFactory`,
which returns an instance of `Zend\Serializer\AdapterPluginManager`. This is a plugin manager for
managing serializer adapter instances.
- `ServiceListener`, mapping to `Zend\Mvc\Service\ServiceListenerFactory`. The factory is used to
instantiate the `ServiceListener`, while allowing easy extending. It checks if a service with the
name `ServiceListenerInterface` exists, which must implement
`Zend\ModuleManager\Listener\ServiceListenerInterface`, before instantiating the default
`ServiceListener`.

        In addition to this, it retrieves the `ApplicationConfig` and looks for the
`service_listener_options` key. This allows you to register own listeners for module methods and
configuration keys to create an own service manager; see the application configuration
        options &lt;zend.mvc.services.app-config&gt; for samples.

- `ValidatorManager`, mapping to `Zend\Mvc\Service\ValidatorManagerFactory`. This instantiates the
`Zend\Validator\ValidatorPluginManager` instance, passing it the service manager instance -- this is
used to manage \[validators\](zend.validator.set). It also uses the `DiAbstractServiceFactory`
service -- effectively allowing you to fall back to DI in order to retrieve validators.
- `ViewFeedRenderer`, mapping to `Zend\Mvc\Service\ViewFeedRendererFactory`, which returns an
instance of `Zend\View\Renderer\FeedRenderer`, used to render feeds.
- `ViewFeedStrategy`, mapping to `Zend\Mvc\Service\ViewFeedStrategyFactory`, which returns an
instance of `Zend\View\Strategy\FeedStrategy`, used to select the `ViewFeedRenderer` given the
appropriate criteria.
- `ViewHelperManager`, mapping to `Zend\Mvc\Service\ViewHelperManagerFactory`, which returns an
instance of `Zend\View\HelperManager`. This is a plugin manager for managing view helper instances.
- `ViewJsonRenderer`, mapping to `Zend\Mvc\Service\ViewJsonRendererFactory`, which returns an
instance of `Zend\View\Renderer\JsonRenderer`, used to render JSON structures.
- `ViewJsonStrategy`, mapping to `Zend\Mvc\Service\ViewJsonStrategyFactory`, which returns an
instance of `Zend\View\Strategy\JsonStrategy`, used to select the `ViewJsonRenderer` given the
appropriate criteria.
- `ViewManager`, mapping to `Zend\Mvc\Service\ViewManagerFactory`. The factory is used to create and
return a view manager, according to the current environment. If the current environment is `cli`, it
will create a `Zend\Mvc\View\Console\ViewManager`, or a `Zend\Mvc\View\Http\ViewManager` if the
current environment is HTTP.
- `ViewResolver`, mapping to `Zend\Mvc\Service\ViewResolverFactory`, which creates and returns the
aggregate view resolver. It also attaches the `ViewTemplateMapResolver` and `ViewTemplatePathStack`
services to it.
- `ViewTemplateMapResolver`, mapping to `Zend\Mvc\Service\ViewTemplateMapResolverFactory` which
creates, configures and returns the `Zend\View\Resolver\TemplateMapResolver`.
- `ViewTemplatePathStack`, mapping to `Zend\Mvc\Service\ViewTemplatePathStackFactory` which creates,
configures and returns the `Zend\View\Resolver\TemplatePathStack`.

- **Abstract factories**
- `Zend\Cache\Service\StorageCacheAbstractServiceFactory` (opt-in; registered by default in the
skeleton application).
- `Zend\Db\Adapter\AdapterAbstractServiceFactory` (opt-in).
- `Zend\Form\FormAbstractServiceFactory` is registered by default.
- `Zend\Log\LoggerAbstractServiceFactory` (opt-in; registered by default in the skeleton
application).
- **Aliases**
- `Configuration`, mapping to the `Config` service.
- `Console`, mapping to the `ConsoleAdapter` service.
- `Di`, mapping to the `DependencyInjector` service.
- `Zend\Di\LocatorInterface`, mapping to the `DependencyInjector` service.
- `Zend\EventManager\EventManagerInterface`, mapping to the `EventManager` service. This is mainly
to ensure that when falling through to DI, classes are still injected via the `ServiceManager`.
- `Zend\Mvc\Controller\PluginManager`, mapping to the `ControllerPluginManager` service. This is
mainly to ensure that when falling through to DI, classes are still injected via the
`ServiceManager`.
- `Zend\View\Resolver\TemplateMapResolver`, mapping to the `ViewTemplateMapResolver` service.
- `Zend\View\Resolver\TemplatePathStack`, mapping to the `ViewTemplatePathStack` service.
- `Zend\View\Resolver\AggregateResolver`, mapping to the `ViewResolver` service.
- `Zend\View\Resolver\ResolverInterface`, mapping to the `ViewResolver` service.
- **Initializers**
- For objects that implement `Zend\EventManager\EventManagerAwareInterface`, the `EventManager`
service will be retrieved and injected. This service is **not** shared, though each instance it
creates is injected with a shared instance of `SharedEventManager`.
- For objects that implement `Zend\ServiceManager\ServiceLocatorAwareInterface`, the
`ServiceManager` will inject itself into the object.
- The `ServiceManager` registers itself as the `ServiceManager` service, and aliases itself to the
class names `Zend\ServiceManager\ServiceLocatorInterface` and `Zend\ServiceManager\ServiceManager`.

## Abstract Factories

As noted in the previous section, Zend Framework provides a number of abstract service factories by
default. Each is noted below, along with sample configuration.

In each instance, the abstract factory looks for a top-level configuration key, consisting of
key/value pairs where the key is the service name, and the value is the configuration to use to
create the given service.

### Zend\\Cache\\Service\\StorageCacheAbstractServiceFactory

This abstract factory is opt-in, but registered by default in the skeleton application. It uses the
top-level configuration key "caches".

```php
return array(
    'caches' => array(
        'Cache\Transient' => array(
            'adapter' => 'redis',
            'ttl'     => 60,
            'plugins' => array(
                'exception_handler' => array(
                    'throw_exceptions' => false,
                ),
            ),
        ),
        'Cache\Persistence' => array(
            'adapter' => 'filesystem',
            'ttl'     => 86400,
        ),
    ),
);
```

See the \[cache documentation\](zend.cache.storage.adapter) for more configuration options.

### Zend\\Db\\Adapter\\AdapterAbstractServiceFactory

This abstract factory is opt-in. It uses the top-level configuration key "db", with a subkey
"adapters".

```php
return array(
    'db' => array('adapters' => array(
        'Db\ReadOnly' => array(
            'driver'   => 'Pdo_Sqlite',
            'database' => 'data/db/users.db',
        ),
        'Db\Writeable' => array(
            'driver'   => 'Mysqli',
            'database' => 'users',
            'username' => 'developer',
            'password' => 'developer_password',
        ),
    )),
);
```

See the \[DB adapter documentation\](zend.db.adapter) for more configuration options.

### Zend\\Form\\FormAbstractServiceFactory

This abstract factory is registered by default. It uses the top-level configuration key "forms". It
makes use of the `FilterManager`, `FormElementManager`, `HydratorManager`, `InputFilterManager`, and
`ValidatorManager` plugin managers in order to allow instantiation and creation of form objects and
all related objects in the form hierarchy.

```php
return array(
    'forms' => array(
        'Form\Foo' => array(
            'hydrator' => 'ObjectProperty',
            'type'     => 'Zend\Form\Form',
            'elements' => array(
                array(
                    'spec' => array(
                        'type' => 'Zend\Form\Element\Email',
                        'name' => 'email',
                        'options' => array(
                            'label' => 'Your email address',
                        ),
                    ),
                ),
            ),
        ),
    ),
);
```

Form configuration follows the same configuration you would use with a form factory; the primary
difference is that all plugin managers have already been injected for you, allowing you the
possibility of custom objects or substitutions.

See the \[form factory documentation\](zend.form.quick-start.factory) for more configuration
options.

### Zend\\Log\\LoggerAbstractServiceFactory

This abstract factory is opt-in, but registered by default in the skeleton application. It uses the
top-level configuration key "log".

```php
return array(
    'log' => array(
        'Log\App' => array(
            'writers' => array(
                array(
                    'name' => 'stream',
                    'priority' => 1000,
                    'options' => array(
                        'stream' => 'data/logs/app.log',
                    ),
                ),
            ),
        ),
    ),
);
```

See the \[log documentation\](zend.log.overview) for more configuration options.

## Plugin Managers

The following plugin managers are configured by default:

- **ControllerManager**, corresponding to `Zend\Mvc\Controller\ControllerManager`, and used to
manage controller instances.
- **ControllerPluginManager**, corresponding to `Zend\Mvc\Controller\PluginManager`, and used to
manage controller plugin instances.
- **FilterManager**, corresponding to `Zend\Filter\FilterPluginManager`, and used to manage filter
instances.
- **FormElementManager**, corresponding to `Zend\Form\FormElementManager`, and used to manage
instances of form elements and fieldsets.
- **HydratorManager**, corresponding to `Zend\Stdlib\Hydrator\HydratorPluginManager`, and used to
manage hydrator instances.
- **InputFilterManager**, corresponding to `Zend\InputFilter\InputFilterPluginManager`, and used to
manage input filter instances.
- **RoutePluginManager**, corresponding to `Zend\Mvc\Router\RoutePluginManager`, and used to manage
route instances.
- **SerializerAdapterManager**, corresponding to `Zend\Serializer\AdapterPluginManager`, and used to
manage serializer instances.
- **ValidatorManager**, corresponding to `Zend\Validator\ValidatorPluginManager`, and used to manage
validator instances.
- **ViewHelperManager**, corresponding to `Zend\View\HelperPluginManager`, and used to manage view
helper instances.

As noted in the previous section, all plugin managers share the same configuration and service types
as the standard service manager; they are simply scoped, and only allow instances of certain types
to be created or registered. Default types available are listed in the documentation for each
component.

## ViewManager

The View layer within `Zend\Mvc` consists of a large number of collaborators and event listeners. As
such, `Zend\Mvc\View\ViewManager` was created to handle creation of the various objects, as well as
wiring them together and establishing event listeners.

The `ViewManager` itself is an event listener on the `bootstrap` event. It retrieves the
`ServiceManager` from the `Application` object, as well as its composed `EventManager`.

Configuration for all members of the `ViewManager` fall under the `view_manager` configuration key,
and expect values as noted below. The following services are created and managed by the
`ViewManager`:

- `ViewHelperManager`, representing and aliased to `Zend\View\HelperPluginManager`. It is seeded
with the `ServiceManager`. Created via the `Zend\Mvc\Service\ViewHelperManagerFactory`.
- The `Router` service is retrieved, and injected into the `Url` helper.
- If the `base_path` key is present, it is used to inject the `BasePath` view helper; otherwise, the
`Request` service is retrieved, and the value of its `getBasePath()` method is used.
- If the `base_path_console` key is present, it is used to inject the `BasePath` view helper for
console requests; otherwise, the `Request` service is retrieved, and the value of its
`getBasePath()` method is used. This can be useful for sending urls in emails via a cronjob.
- If the `doctype` key is present, it will be used to set the value of the `Doctype` view helper.
- `ViewTemplateMapResolver`, representing and aliased to `Zend\View\Resolver\TemplateMapResolver`.
If a `template_map` key is present, it will be used to seed the template map.
- `ViewTemplatePathStack`, representing and aliased to `Zend\View\Resolver\TemplatePathStack`.
- If a `template_path_stack` key is present, it will be used to seed the stack.
- If a `default_template_suffix` key is present, it will be used as the default suffix for template
scripts resolving.
- `ViewResolver`, representing and aliased to `Zend\View\Resolver\AggregateResolver` and
`Zend\View\Resolver\ResolverInterface`. It is seeded with the `ViewTemplateMapResolver` and
`ViewTemplatePathStack` services as resolvers.
- `ViewRenderer`, representing and aliased to `Zend\View\Renderer\PhpRenderer` and
`Zend\View\Renderer\RendererInterface`. It is seeded with the `ViewResolver` and `ViewHelperManager`
services. Additionally, the `ViewModel` helper gets seeded with the `ViewModel` as its root (layout)
model.
- `ViewPhpRendererStrategy`, representing and aliased to `Zend\View\Strategy\PhpRendererStrategy`.
It gets seeded with the `ViewRenderer` service.
- `View`, representing and aliased to `Zend\View\View`. It gets seeded with the `EventManager`
service, and attaches the `ViewPhpRendererStrategy` as an aggregate listener.
- `DefaultRenderingStrategy`, representing and aliased to `Zend\Mvc\View\DefaultRenderingStrategy`.
If the `layout` key is present, it is used to seed the strategy's layout template. It is seeded with
the `View` service.
- `ExceptionStrategy`, representing and aliased to `Zend\Mvc\View\ExceptionStrategy`. If the
`display_exceptions` or `exception_template` keys are present, they are used to configure the
strategy.
- `RouteNotFoundStrategy`, representing and aliased to `Zend\Mvc\View\RouteNotFoundStrategy` and
`404Strategy`. If the `display_not_found_reason` or `not_found_template` keys are present, they are
used to configure the strategy.
- `ViewModel`. In this case, no service is registered; the `ViewModel` is simply retrieved from the
`MvcEvent` and injected with the layout template name.

The `ViewManager` also creates several other listeners, but does not expose them as services; these
include `Zend\Mvc\View\CreateViewModelListener`, `Zend\Mvc\View\InjectTemplateListener`, and
`Zend\Mvc\View\InjectViewModelListener`. These, along with `RouteNotFoundStrategy`,
`ExceptionStrategy`, and `DefaultRenderingStrategy` are attached as listeners either to the
application `EventManager` instance or the `SharedEventManager` instance.

Finally, if you have a `strategies` key in your configuration, the `ViewManager` will loop over
these and attach them in order to the `View` service as listeners, at a priority of 100 (allowing
them to execute before the `DefaultRenderingStrategy`).

## Application Configuration Options

The following options may be used to provide initial configuration for the `ServiceManager`,
`ModuleManager`, and `Application` instances, allowing them to then find and aggregate the
configuration used for the `Config` service, which is intended for configuring all other objects in
the system. These configuration directives go to the `config/application.config.php` file.

```php
<?php
return array(
    // This should be an array of module namespaces used in the application.
    'modules' => array(
    ),

    // These are various options for the listeners attached to the ModuleManager
    'module_listener_options' => array(
        // This should be an array of paths in which modules reside.
        // If a string key is provided, the listener will consider that a module
        // namespace, the value of that key the specific path to that module's
        // Module class.
        'module_paths' => array(
        ),

        // An array of paths from which to glob configuration files after
        // modules are loaded. These effectively override configuration
        // provided by modules themselves. Paths may use GLOB_BRACE notation.
        'config_glob_paths' => array(
        ),

        // Whether or not to enable a configuration cache.
        // If enabled, the merged configuration will be cached and used in
        // subsequent requests.
        'config_cache_enabled' => $booleanValue,

        // The key used to create the configuration cache file name.
        'config_cache_key' => $stringKey,

        // Whether or not to enable a module class map cache.
        // If enabled, creates a module class map cache which will be used
        // by in future requests, to reduce the autoloading process.
        'module_map_cache_enabled' => $booleanValue,

        // The key used to create the class map cache file name.
        'module_map_cache_key' => $stringKey,

        // The path in which to cache merged configuration.
        'cache_dir' => $stringPath,

        // Whether or not to enable modules dependency checking.
        // Enabled by default, prevents usage of modules that depend on other modules
        // that weren't loaded.
        'check_dependencies' => $booleanValue,
    ),

    // Used to create an own service manager. May contain one or more child arrays.
    'service_listener_options' => array(
       array(
         'service_manager' => $stringServiceManagerName,
         'config_key'      => $stringConfigKey,
         'interface'       => $stringOptionalInterface,
         'method'          => $stringRequiredMethodName,
       ),
    )

    // Initial configuration with which to seed the ServiceManager.
    // Should be compatible with Zend\ServiceManager\Config.
    'service_manager' => array(
    ),
);
```

For an example, see the [ZendSkeletonApplication configuration
file](https://github.com/zendframework/ZendSkeletonApplication/blob/master/config/application.config.php).

## Default Configuration Options

The following options are available when using the default services configured by the
`ServiceManagerConfig` and `ViewManager`.

These configuration directives can go to the `config/autoload/{{,*.}global,{,*.}local}.php` files,
or in the `module/<module name>/config/module.config.php` configuration files. The merging of these
configuration files is done by the `ModuleManager`. It first merges each module's
`module.config.php` file, and then the files in `config/autoload` (first the `*.global.php` and then
the `*.local.php` files). The order of the merge is relevant so you can override a module's
configuration with your application configuration. If you have both a
`config/autoload/my.global.config.php` and `config/autoload/my.local.config.php`, the local
configuration file overrides the global configuration.

> ## Warning
Local configuration files are intended to keep sensitive information, such as database credentials,
and as such, it is highly recommended to keep these local configuration files out of your VCS. The
`ZendSkeletonApplication`'s `config/autoload/.gitignore` file ignores `*.local.php` files by
default.

```php
<?php
return array(
    // The following are used to configure controller loader
    // Should be compatible with Zend\ServiceManager\Config.
    'controllers' => array(
        // Map of controller "name" to class
        // This should be used if you do not need to inject any dependencies
        // in your controller
        'invokables' => array(
        ),

        // Map of controller "name" to factory for creating controller instance
        // You may provide either the class name of a factory, or a PHP callback.
        'factories' => array(
        ),
    ),

    // The following are used to configure controller plugin loader
    // Should be compatible with Zend\ServiceManager\Config.
    'controller_plugins' => array(
    ),

    // The following are used to configure view helper manager
    // Should be compatible with Zend\ServiceManager\Config.
    'view_helpers' => array(
    ),

    // The following is used to configure a Zend\Di\Di instance.
    // The array should be in a format that Zend\Di\Config can understand.
    'di' => array(
    ),

    // Configuration for the Router service
    // Can contain any router configuration, but typically will always define
    // the routes for the application. See the router documentation for details
    // on route configuration.
    'router' => array(
        'routes' => array(
        ),
    ),

    // ViewManager configuration
    'view_manager' => array(
        // Base URL path to the application
        'base_path' => $stringBasePath,

        // Doctype with which to seed the Doctype helper
        'doctype' => $doctypeHelperConstantString, // e.g. HTML5, XHTML1

        // TemplateMapResolver configuration
        // template/path pairs
        'template_map' => array(
        ),

        // TemplatePathStack configuration
        // module/view script path pairs
        'template_path_stack' => array(
        ),
        // Default suffix to use when resolving template scripts, if none, 'phtml' is used
        'default_template_suffix' => $templateSuffix, // e.g. 'php'

        // Controller namespace to template map
        // or whitelisting for controller FQCN to template mapping
        'controller_map' => array(
        ),

        // Layout template name
        'layout' => $layoutTemplateName, // e.g. 'layout/layout'

        // ExceptionStrategy configuration
        'display_exceptions' => $bool, // display exceptions in template
        'exception_template' => $stringTemplateName, // e.g. 'error'

        // RouteNotFoundStrategy configuration
        'display_not_found_reason' => $bool, // display 404 reason in template
        'not_found_template' => $stringTemplateName, // e.g. '404'

        // Additional strategies to attach
        // These should be class names or service names of View strategy classes
        // that act as ListenerAggregates. They will be attached at priority 100,
        // in the order registered.
        'strategies' => array(
            'ViewJsonStrategy', // register JSON renderer strategy
            'ViewFeedStrategy', // register Feed renderer strategy
        ),
    ),
);
```

For an example, see the [Application module configuration
file](https://github.com/zendframework/ZendSkeletonApplication/blob/master/module/Application/config/module.config.php)
in the ZendSkeletonApplication.

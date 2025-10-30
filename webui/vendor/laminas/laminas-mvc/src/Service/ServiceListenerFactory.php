<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\ModuleManager\Listener\ServiceListener;
use Laminas\ModuleManager\Listener\ServiceListenerInterface;
use Laminas\Mvc\Exception\InvalidArgumentException;
use Laminas\Mvc\Exception\RuntimeException;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class ServiceListenerFactory implements FactoryInterface
{
    /**
     * @var string
     */
    const MISSING_KEY_ERROR = 'Invalid service listener options detected, %s array must contain %s key.';

    /**
     * @var string
     */
    const VALUE_TYPE_ERROR = 'Invalid service listener options detected, %s must be a string, %s given.';

    /**
     * Default mvc-related service configuration -- can be overridden by modules.
     *
     * @var array
     */
    protected $defaultServiceConfig = [
        'invokables' => [
            'DispatchListener'     => 'Laminas\Mvc\DispatchListener',
            'RouteListener'        => 'Laminas\Mvc\RouteListener',
            'SendResponseListener' => 'Laminas\Mvc\SendResponseListener',
            'ViewJsonRenderer'     => 'Laminas\View\Renderer\JsonRenderer',
            'ViewFeedRenderer'     => 'Laminas\View\Renderer\FeedRenderer',
        ],
        'factories' => [
            'Application'                    => 'Laminas\Mvc\Service\ApplicationFactory',
            'Config'                         => 'Laminas\Mvc\Service\ConfigFactory',
            'ControllerLoader'               => 'Laminas\Mvc\Service\ControllerLoaderFactory',
            'ControllerPluginManager'        => 'Laminas\Mvc\Service\ControllerPluginManagerFactory',
            'ConsoleAdapter'                 => 'Laminas\Mvc\Service\ConsoleAdapterFactory',
            'ConsoleRouter'                  => 'Laminas\Mvc\Service\RouterFactory',
            'ConsoleViewManager'             => 'Laminas\Mvc\Service\ConsoleViewManagerFactory',
            'DependencyInjector'             => 'Laminas\Mvc\Service\DiFactory',
            'DiAbstractServiceFactory'       => 'Laminas\Mvc\Service\DiAbstractServiceFactoryFactory',
            'DiServiceInitializer'           => 'Laminas\Mvc\Service\DiServiceInitializerFactory',
            'DiStrictAbstractServiceFactory' => 'Laminas\Mvc\Service\DiStrictAbstractServiceFactoryFactory',
            'FilterManager'                  => 'Laminas\Mvc\Service\FilterManagerFactory',
            'FormAnnotationBuilder'          => 'Laminas\Mvc\Service\FormAnnotationBuilderFactory',
            'FormElementManager'             => 'Laminas\Mvc\Service\FormElementManagerFactory',
            'HttpRouter'                     => 'Laminas\Mvc\Service\RouterFactory',
            'HttpMethodListener'             => 'Laminas\Mvc\Service\HttpMethodListenerFactory',
            'HttpViewManager'                => 'Laminas\Mvc\Service\HttpViewManagerFactory',
            'HydratorManager'                => 'Laminas\Mvc\Service\HydratorManagerFactory',
            'InjectTemplateListener'         => 'Laminas\Mvc\Service\InjectTemplateListenerFactory',
            'InputFilterManager'             => 'Laminas\Mvc\Service\InputFilterManagerFactory',
            'LogProcessorManager'            => 'Laminas\Mvc\Service\LogProcessorManagerFactory',
            'LogWriterManager'               => 'Laminas\Mvc\Service\LogWriterManagerFactory',
            'MvcTranslator'                  => 'Laminas\Mvc\Service\TranslatorServiceFactory',
            'PaginatorPluginManager'         => 'Laminas\Mvc\Service\PaginatorPluginManagerFactory',
            'Request'                        => 'Laminas\Mvc\Service\RequestFactory',
            'Response'                       => 'Laminas\Mvc\Service\ResponseFactory',
            'Router'                         => 'Laminas\Mvc\Service\RouterFactory',
            'RoutePluginManager'             => 'Laminas\Mvc\Service\RoutePluginManagerFactory',
            'SerializerAdapterManager'       => 'Laminas\Mvc\Service\SerializerAdapterPluginManagerFactory',
            'TranslatorPluginManager'        => 'Laminas\Mvc\Service\TranslatorPluginManagerFactory',
            'ValidatorManager'               => 'Laminas\Mvc\Service\ValidatorManagerFactory',
            'ViewHelperManager'              => 'Laminas\Mvc\Service\ViewHelperManagerFactory',
            'ViewFeedStrategy'               => 'Laminas\Mvc\Service\ViewFeedStrategyFactory',
            'ViewJsonStrategy'               => 'Laminas\Mvc\Service\ViewJsonStrategyFactory',
            'ViewManager'                    => 'Laminas\Mvc\Service\ViewManagerFactory',
            'ViewResolver'                   => 'Laminas\Mvc\Service\ViewResolverFactory',
            'ViewTemplateMapResolver'        => 'Laminas\Mvc\Service\ViewTemplateMapResolverFactory',
            'ViewTemplatePathStack'          => 'Laminas\Mvc\Service\ViewTemplatePathStackFactory',
            'ViewPrefixPathStackResolver'    => 'Laminas\Mvc\Service\ViewPrefixPathStackResolverFactory',
        ],
        'aliases' => [
            'Configuration'                              => 'Config',
            'Console'                                    => 'ConsoleAdapter',
            'Di'                                         => 'DependencyInjector',
            'Laminas\Di\LocatorInterface'                   => 'DependencyInjector',
            'Laminas\Form\Annotation\FormAnnotationBuilder' => 'FormAnnotationBuilder',
            'Laminas\Mvc\Controller\PluginManager'          => 'ControllerPluginManager',
            'Laminas\Mvc\View\Http\InjectTemplateListener'  => 'InjectTemplateListener',
            'Laminas\View\Resolver\TemplateMapResolver'     => 'ViewTemplateMapResolver',
            'Laminas\View\Resolver\TemplatePathStack'       => 'ViewTemplatePathStack',
            'Laminas\View\Resolver\AggregateResolver'       => 'ViewResolver',
            'Laminas\View\Resolver\ResolverInterface'       => 'ViewResolver',
            'ControllerManager'                          => 'ControllerLoader',
        ],
        'abstract_factories' => [
            'Laminas\Form\FormAbstractServiceFactory',
        ],
    ];

    /**
     * Create the service listener service
     *
     * Tries to get a service named ServiceListenerInterface from the service
     * locator, otherwise creates a Laminas\ModuleManager\Listener\ServiceListener
     * service, passing it the service locator instance and the default service
     * configuration, which can be overridden by modules.
     *
     * It looks for the 'service_listener_options' key in the application
     * config and tries to add service manager as configured. The value of
     * 'service_listener_options' must be a list (array) which contains the
     * following keys:
     *   - service_manager: the name of the service manage to create as string
     *   - config_key: the name of the configuration key to search for as string
     *   - interface: the name of the interface that modules can implement as string
     *   - method: the name of the method that modules have to implement as string
     *
     * @param  ServiceLocatorInterface  $serviceLocator
     * @return ServiceListener
     * @throws InvalidArgumentException For invalid configurations.
     * @throws RuntimeException
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $configuration   = $serviceLocator->get('ApplicationConfig');

        if ($serviceLocator->has('ServiceListenerInterface')) {
            $serviceListener = $serviceLocator->get('ServiceListenerInterface');

            if (!$serviceListener instanceof ServiceListenerInterface) {
                throw new RuntimeException(
                    'The service named ServiceListenerInterface must implement ' .
                    'Laminas\ModuleManager\Listener\ServiceListenerInterface'
                );
            }

            $serviceListener->setDefaultServiceConfig($this->defaultServiceConfig);
        } else {
            $serviceListener = new ServiceListener($serviceLocator, $this->defaultServiceConfig);
        }

        if (isset($configuration['service_listener_options'])) {
            if (!is_array($configuration['service_listener_options'])) {
                throw new InvalidArgumentException(sprintf(
                    'The value of service_listener_options must be an array, %s given.',
                    gettype($configuration['service_listener_options'])
                ));
            }

            foreach ($configuration['service_listener_options'] as $key => $newServiceManager) {
                if (!isset($newServiceManager['service_manager'])) {
                    throw new InvalidArgumentException(sprintf(self::MISSING_KEY_ERROR, $key, 'service_manager'));
                } elseif (!is_string($newServiceManager['service_manager'])) {
                    throw new InvalidArgumentException(sprintf(
                        self::VALUE_TYPE_ERROR,
                        'service_manager',
                        gettype($newServiceManager['service_manager'])
                    ));
                }
                if (!isset($newServiceManager['config_key'])) {
                    throw new InvalidArgumentException(sprintf(self::MISSING_KEY_ERROR, $key, 'config_key'));
                } elseif (!is_string($newServiceManager['config_key'])) {
                    throw new InvalidArgumentException(sprintf(
                        self::VALUE_TYPE_ERROR,
                        'config_key',
                        gettype($newServiceManager['config_key'])
                    ));
                }
                if (!isset($newServiceManager['interface'])) {
                    throw new InvalidArgumentException(sprintf(self::MISSING_KEY_ERROR, $key, 'interface'));
                } elseif (!is_string($newServiceManager['interface'])) {
                    throw new InvalidArgumentException(sprintf(
                        self::VALUE_TYPE_ERROR,
                        'interface',
                        gettype($newServiceManager['interface'])
                    ));
                }
                if (!isset($newServiceManager['method'])) {
                    throw new InvalidArgumentException(sprintf(self::MISSING_KEY_ERROR, $key, 'method'));
                } elseif (!is_string($newServiceManager['method'])) {
                    throw new InvalidArgumentException(sprintf(
                        self::VALUE_TYPE_ERROR,
                        'method',
                        gettype($newServiceManager['method'])
                    ));
                }

                $serviceListener->addServiceManager(
                    $newServiceManager['service_manager'],
                    $newServiceManager['config_key'],
                    $newServiceManager['interface'],
                    $newServiceManager['method']
                );
            }
        }

        return $serviceListener;
    }
}

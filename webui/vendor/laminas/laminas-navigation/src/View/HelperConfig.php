<?php

/**
 * @see       https://github.com/laminas/laminas-navigation for the canonical source repository
 * @copyright https://github.com/laminas/laminas-navigation/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-navigation/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Navigation\View;

use Laminas\ServiceManager\Config;
use Laminas\ServiceManager\Factory\InvokableFactory;
use Laminas\ServiceManager\ServiceManager;
use Laminas\Stdlib\ArrayUtils;
use Laminas\View\Helper\Navigation as NavigationHelper;
use ReflectionProperty;
use Traversable;

/**
 * Service manager configuration for navigation view helpers
 */
class HelperConfig extends Config
{
    /**
     * Default configuration to apply.
     *
     * @var string[][]
     */
    protected $config = [
        'abstract_factories' => [],
        'aliases' => [
            'navigation' => NavigationHelper::class,
            'Navigation' => NavigationHelper::class,
        ],
        'delegators' => [],
        'factories' => [
            NavigationHelper::class    => NavigationHelperFactory::class,
            'laminasviewhelpernavigation' => NavigationHelperFactory::class,
        ],
        'initializers'  => [],
        'invokables'    => [],
        'lazy_services' => [],
        'services'      => [],
        'shared'        => [],
    ];

    /**
     * Navigation helper delegator factory.
     *
     * @var callable
     */
    protected $navigationDelegatorFactory;

    /**
     * Constructor.
     *
     * Ensure incoming configuration is *merged* with the defaults defined.
     *
     * @param array
     */
    public function __construct(array $config = [])
    {
        $this->mergeConfig($config);
    }

    /**
     * Configure the provided container.
     *
     * Merges navigation_helpers configuration from the parent containers
     * config service with the configuration in this class, and uses that to
     * configure the provided service container (which should be the laminas-view
     * `HelperPluginManager`).  with the service locator instance.
     *
     * Before configuring he provided container, it also adds a delegator
     * factory for the `Navigation` helper; the delegator uses the configuration
     * from this class to seed the `PluginManager` used by the `NavigationHelper`,
     * ensuring that any overrides provided via configuration are propagated
     * to it.
     *
     * @param  ServiceManager $serviceManager
     * @return ServiceManager
     */
    public function configureServiceManager(ServiceManager $container)
    {
        $services = $this->getParentContainer($container);

        if ($services->has('config')) {
            $this->mergeHelpersFromConfiguration($services->get('config'));
        }

        $this->injectNavigationDelegatorFactory(method_exists($container, 'configure'));

        parent::configureServiceManager($container);

        return $container;
    }

    /**
     * Merge an array of configuration with the settings already present.
     *
     * Processes invokables as invokable factories and optionally additional
     * aliases.
     *
     * @param array $config
     * @return void
     */
    private function mergeConfig(array $config)
    {
        if (isset($config['invokables'])) {
            $config = $this->processInvokables($config['invokables'], $config);
        }

        foreach ($config as $type => $services) {
            if (isset($this->config[$type])) {
                $this->config[$type] = ArrayUtils::merge($this->config[$type], $services);
            }
        }
    }

    /**
     * Merge navigation helper configuration with default configuration.
     *
     * @param array|Traversable $config
     * @return void
     */
    private function mergeHelpersFromConfiguration($config)
    {
        if ($config instanceof Traversable) {
            $config = iterator_to_array($config);
        }

        if (! isset($config['navigation_helpers'])
            || (! is_array($config['navigation_helpers']) && ! $config['navigation_helpers'] instanceof Traversable)
        ) {
            return;
        }

        $this->mergeConfig($config['navigation_helpers']);
    }

    /**
     * Retrieve the parent container from the plugin manager, if possible.
     *
     * @param ServiceManager $container
     * @return ServiceManager
     */
    private function getParentContainer(ServiceManager $container)
    {
        // We need the parent container in order to retrieve the config
        // service. We should likely revisit how this is done in the future.
        //
        // v3:
        if (method_exists($container, 'configure')) {
            $r = new ReflectionProperty($container, 'creationContext');
            $r->setAccessible(true);
            return $r->getValue($container) ?: $container;
        }

        // v2:
        return $container->getServiceLocator() ?: $container;
    }

    /**
     * Normalizes a factory service name for use with laminas-servicemanager v2.
     *
     * @param string $name
     * @return string
     */
    private function normalizeNameForV2($name)
    {
        return strtolower(strtr($name, ['-' => '', '_' => '', ' ' => '', '\\' => '', '/' => '']));
    }

    /**
     * Process invokables in order to seed aliases and factories.
     *
     * @param array $invokables Array of invokables defined
     * @param array $config All service configuration
     * @return array Array of all service configuration
     */
    private function processInvokables(array $invokables, array $config)
    {
        if (! isset($config['aliases'])) {
            $config['aliases'] = [];
        }

        if (! isset($config['factories'])) {
            $config['factories'] = [];
        }

        foreach ($invokables as $name => $class) {
            $config['factories'][$class] = InvokableFactory::class;
            $config['factories'][$this->normalizeNameForV2($class)] = InvokableFactory::class;

            if ($name === $class) {
                continue;
            }

            $config['aliases'][$name] = $class;
        }

        unset($config['invokables']);

        return $config;
    }

    /**
     * Inject the navigation helper delegator factory into the configuration.
     *
     * @param bool $isV3Container
     * @return void
     */
    private function injectNavigationDelegatorFactory($isV3Container)
    {
        $factory = $this->prepareNavigationDelegatorFactory($isV3Container);

        if (isset($this->config['delegators'][NavigationHelperFactory::class])
            && in_array($factory, $this->config['delegators'][NavigationHelperFactory::class], true)
        ) {
            // Already present
            return;
        }

        // Inject the delegator factory
        $this->config['delegators'][NavigationHelper::class][] = $factory;
        $this->config['delegators']['laminasviewhelpernavigation'][] = $factory;
    }

    /**
     * Return a delegator factory that configures the navigation plugin manager
     * with the configuration in this class.
     *
     * @param bool $isV3Container
     * @return callable
     */
    private function prepareNavigationDelegatorFactory($isV3Container)
    {
        if (isset($this->navigationDelegatorFactory)) {
            return $this->navigationDelegatorFactory;
        }

        $this->navigationDelegatorFactory = $isV3Container
            ? $this->prepareV3NavigationDelegatorFactory($this->config)
            : $this->prepareV2NavigationDelegatorFactory($this->config);

        return $this->navigationDelegatorFactory;
    }

    /**
     * Return a delegator factory compatible with v2
     *
     * @param array $config Configuration to use when configuring the
     *     navigation plugin manager.
     * @return callable
     */
    private function prepareV2NavigationDelegatorFactory(array $config)
    {
        return function ($container, $canonicalName, $requestedName, $callback) use ($config) {
            $helper = $callback();

            $pluginManager = $helper->getPluginManager();
            (new Config($config))->configureServiceManager($pluginManager);

            return $helper;
        };
    }

    /**
     * Return a delegator factory compatible with v3
     *
     * @param array $config Configuration to use when configuring the
     *     navigation plugin manager.
     * @return callable
     */
    private function prepareV3NavigationDelegatorFactory(array $config)
    {
        return function ($container, $name, $callback, $options) use ($config) {
            $helper = $callback();

            $pluginManager = $helper->getPluginManager();
            (new Config($config))->configureServiceManager($pluginManager);

            return $helper;
        };
    }
}

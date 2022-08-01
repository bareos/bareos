<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Mvc\Service;

use Interop\Container\ContainerInterface;
use Zend\Console\Console;
use Zend\Mvc\Exception;
use Zend\Mvc\Router\RouteMatch;
use Zend\ServiceManager\ConfigInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Zend\View\Helper as ViewHelper;
use Zend\View\HelperPluginManager;
use Zend\View\Helper\HelperInterface as ViewHelperInterface;

class ViewHelperManagerFactory extends AbstractPluginManagerFactory
{
    const PLUGIN_MANAGER_CLASS = HelperPluginManager::class;

    /**
     * An array of helper configuration classes to ensure are on the helper_map stack.
     *
     * @var array
     */
    protected $defaultHelperMapClasses = [
        'Zend\Form\View\HelperConfig',
        'Zend\I18n\View\HelperConfig',
        'Zend\Navigation\View\HelperConfig'
    ];

    /**
     * Create and return the view helper manager
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return ViewHelperInterface
     * @throws Exception\RuntimeException
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $plugins = parent::createService($serviceLocator);

        // Configure default helpers from other components
        $plugins = $this->configureHelpers($plugins);

        // Override plugin factories
        $plugins = $this->injectOverrideFactories($plugins, $serviceLocator);

        return $plugins;
    }

    /**
     * Configure helpers from other components.
     *
     * Loops through the list of default helper configuration classes, and uses
     * each to configure the helper plugin manager.
     *
     * @param HelperPluginManager $plugins
     * @return HelperPluginManager
     */
    private function configureHelpers(HelperPluginManager $plugins)
    {
        foreach ($this->defaultHelperMapClasses as $configClass) {
            if (! is_string($configClass) || ! class_exists($configClass)) {
                continue;
            }

            $config = new $configClass;

            if (! $config instanceof ConfigInterface) {
                throw new Exception\RuntimeException(sprintf(
                    'Invalid service manager configuration class provided; received "%s", expected class implementing %s',
                    $configClass,
                    'Zend\ServiceManager\ConfigInterface'
                ));
            }

            $config->configureServiceManager($plugins);
        }

        return $plugins;
    }

    /**
     * Inject override factories into the plugin manager.
     *
     * @param HelperPluginManager $plugins
     * @param ContainerInterface $services
     * @return HelperPluginManager
     */
    private function injectOverrideFactories(HelperPluginManager $plugins, ContainerInterface $services)
    {
        // Configure URL view helper
        $urlFactory = $this->createUrlHelperFactory($services);
        $plugins->setFactory(ViewHelper\Url::class, $urlFactory);
        $plugins->setFactory('zendviewhelperurl', $urlFactory);

        // Configure base path helper
        $basePathFactory = $this->createBasePathHelperFactory($services);
        $plugins->setFactory(ViewHelper\BasePath::class, $basePathFactory);
        $plugins->setFactory('zendviewhelperbasepath', $basePathFactory);

        // Configure doctype view helper
        $doctypeFactory = $this->createDoctypeHelperFactory($services);
        $plugins->setFactory(ViewHelper\doctype::class, $doctypeFactory);
        $plugins->setFactory('zendviewhelperdoctype', $doctypeFactory);

        return $plugins;
    }

    /**
     * Create and return a factory for creating a URL helper.
     *
     * Retrieves the application and router from the servicemanager,
     * and the route match from the MvcEvent composed by the application,
     * using them to configure the helper.
     *
     * @param ContainerInterface $services
     * @return callable
     */
    private function createUrlHelperFactory(ContainerInterface $services)
    {
        return function () use ($services) {
            $helper = new ViewHelper\Url;
            $router = Console::isConsole() ? 'HttpRouter' : 'Router';
            $helper->setRouter($services->get($router));

            $match = $services->get('application')
                ->getMvcEvent()
                ->getRouteMatch()
            ;

            if ($match instanceof RouteMatch) {
                $helper->setRouteMatch($match);
            }

            return $helper;
        };
    }

    /**
     * Create and return a factory for creating a BasePath helper.
     *
     * Uses configuration and request services to configure the helper.
     *
     * @param ContainerInterface $services
     * @return callable
     */
    private function createBasePathHelperFactory(ContainerInterface $services)
    {
        return function () use ($services) {
            $config = $services->has('config') ? $services->get('config') : [];
            $basePathHelper = new ViewHelper\BasePath;

            if (Console::isConsole()
                && isset($config['view_manager'])
                && isset($config['view_manager']['base_path_console'])
            ) {
                $basePathHelper->setBasePath($config['view_manager']['base_path_console']);

                return $basePathHelper;
            }

            if (isset($config['view_manager']) && isset($config['view_manager']['base_path'])) {
                $basePathHelper->setBasePath($config['view_manager']['base_path']);

                return $basePathHelper;
            }

            $request = $services->get('Request');

            if (is_callable([$request, 'getBasePath'])) {
                $basePathHelper->setBasePath($request->getBasePath());
            }

            return $basePathHelper;
        };
    }

    /**
     * Create and return a Doctype helper factory.
     *
     * Other view helpers depend on this to decide which spec to generate their tags
     * based on. This is why it must be set early instead of later in the layout phtml.
     *
     * @param ContainerInterface $services
     * @return callable
     */
    private function createDoctypeHelperFactory(ContainerInterface $services)
    {
        return function () use ($services) {
            $config = $services->has('config') ? $services->get('config') : [];
            $config = isset($config['view_manager']) ? $config['view_manager'] : [];
            $doctypeHelper = new ViewHelper\Doctype;
            if (isset($config['doctype']) && $config['doctype']) {
                $doctypeHelper->setDoctype($config['doctype']);
            }
            return $doctypeHelper;
        };
    }
}

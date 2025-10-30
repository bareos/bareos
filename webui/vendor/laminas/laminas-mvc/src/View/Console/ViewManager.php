<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\View\Console;

use ArrayAccess;
use Laminas\Mvc\MvcEvent;
use Laminas\Mvc\View\Http\ViewManager as BaseViewManager;

/**
 * Prepares the view layer for console applications
 */
class ViewManager extends BaseViewManager
{
    /**
     * Prepares the view layer
     *
     * Overriding, as several operations are omitted in the console view
     * algorithms, as well as to ensure we pick up the Console variants
     * of several listeners and strategies.
     *
     * @param  \Laminas\Mvc\MvcEvent $event
     * @return void
     */
    public function onBootstrap($event)
    {
        $application    = $event->getApplication();
        $services       = $application->getServiceManager();
        $events         = $application->getEventManager();
        $sharedEvents   = $events->getSharedManager();
        $this->config   = $this->loadConfig($services->get('Config'));
        $this->services = $services;
        $this->event    = $event;

        $routeNotFoundStrategy   = $this->getRouteNotFoundStrategy();
        $exceptionStrategy       = $this->getExceptionStrategy();
        $mvcRenderingStrategy    = $this->getMvcRenderingStrategy();
        $createViewModelListener = new CreateViewModelListener();
        $injectViewModelListener = new InjectViewModelListener();
        $injectParamsListener    = new InjectNamedConsoleParamsListener();

        $this->registerMvcRenderingStrategies($events);
        $this->registerViewStrategies();

        $events->attach($routeNotFoundStrategy);
        $events->attach($exceptionStrategy);
        $events->attach(MvcEvent::EVENT_DISPATCH_ERROR, [$injectViewModelListener, 'injectViewModel'], -100);
        $events->attach(MvcEvent::EVENT_RENDER_ERROR, [$injectViewModelListener, 'injectViewModel'], -100);
        $events->attach($mvcRenderingStrategy);

        $sharedEvents->attach('Laminas\Stdlib\DispatchableInterface', MvcEvent::EVENT_DISPATCH, [$injectParamsListener,  'injectNamedParams'], 1000);
        $sharedEvents->attach('Laminas\Stdlib\DispatchableInterface', MvcEvent::EVENT_DISPATCH, [$createViewModelListener, 'createViewModelFromArray'], -80);
        $sharedEvents->attach('Laminas\Stdlib\DispatchableInterface', MvcEvent::EVENT_DISPATCH, [$createViewModelListener, 'createViewModelFromString'], -80);
        $sharedEvents->attach('Laminas\Stdlib\DispatchableInterface', MvcEvent::EVENT_DISPATCH, [$createViewModelListener, 'createViewModelFromNull'], -80);
        $sharedEvents->attach('Laminas\Stdlib\DispatchableInterface', MvcEvent::EVENT_DISPATCH, [$injectViewModelListener, 'injectViewModel'], -100);
    }

    /**
     * Instantiates and configures the default MVC rendering strategy
     *
     * Overriding to ensure we pick up the MVC rendering strategy for console,
     * as well as to ensure that the appropriate aliases are set.
     *
     * @return DefaultRenderingStrategy
     */
    public function getMvcRenderingStrategy()
    {
        if ($this->mvcRenderingStrategy) {
            return $this->mvcRenderingStrategy;
        }

        $this->mvcRenderingStrategy = new DefaultRenderingStrategy();

        $this->services->setService('DefaultRenderingStrategy', $this->mvcRenderingStrategy);
        $this->services->setAlias('Laminas\Mvc\View\DefaultRenderingStrategy', 'DefaultRenderingStrategy');
        $this->services->setAlias('Laminas\Mvc\View\Console\DefaultRenderingStrategy', 'DefaultRenderingStrategy');

        return $this->mvcRenderingStrategy;
    }

    /**
     * Instantiates and configures the exception strategy
     *
     * Overriding to ensure we pick up the exception strategy for console, as
     * well as to ensure that the appropriate aliases are set.
     *
     * @return ExceptionStrategy
     */
    public function getExceptionStrategy()
    {
        if ($this->exceptionStrategy) {
            return $this->exceptionStrategy;
        }

        $this->exceptionStrategy = new ExceptionStrategy();

        if (isset($this->config['display_exceptions'])) {
            $this->exceptionStrategy->setDisplayExceptions($this->config['display_exceptions']);
        }
        if (isset($this->config['exception_message'])) {
            $this->exceptionStrategy->setMessage($this->config['exception_message']);
        }

        $this->services->setService('ExceptionStrategy', $this->exceptionStrategy);
        $this->services->setAlias('Laminas\Mvc\View\ExceptionStrategy', 'ExceptionStrategy');
        $this->services->setAlias('Laminas\Mvc\View\Console\ExceptionStrategy', 'ExceptionStrategy');

        return $this->exceptionStrategy;
    }

    /**
     * Instantiates and configures the "route not found", or 404, strategy
     *
     * Overriding to ensure we pick up the route not found strategy for console,
     * as well as to ensure that the appropriate aliases are set.
     *
     * @return RouteNotFoundStrategy
     */
    public function getRouteNotFoundStrategy()
    {
        if ($this->routeNotFoundStrategy) {
            return $this->routeNotFoundStrategy;
        }

        $this->routeNotFoundStrategy = new RouteNotFoundStrategy();

        $displayNotFoundReason = true;

        if (array_key_exists('display_not_found_reason', $this->config)) {
            $displayNotFoundReason = $this->config['display_not_found_reason'];
        }
        $this->routeNotFoundStrategy->setDisplayNotFoundReason($displayNotFoundReason);

        $this->services->setService('RouteNotFoundStrategy', $this->routeNotFoundStrategy);
        $this->services->setAlias('Laminas\Mvc\View\RouteNotFoundStrategy', 'RouteNotFoundStrategy');
        $this->services->setAlias('Laminas\Mvc\View\Console\RouteNotFoundStrategy', 'RouteNotFoundStrategy');
        $this->services->setAlias('404Strategy', 'RouteNotFoundStrategy');

        return $this->routeNotFoundStrategy;
    }

    /**
     * Extract view manager configuration from the application's configuration
     *
     * @param array|ArrayAccess $configService
     *
     * @return array
     */
    private function loadConfig($configService)
    {
        $config = [];

        // override when console config is provided, otherwise use the standard definition
        if (isset($configService['console']['view_manager'])) {
            $config = $configService['console']['view_manager'];
        } elseif (isset($configService['view_manager'])) {
            $config = $configService['view_manager'];
        }

        return ($config instanceof ArrayAccess || is_array($config))
            ? $config
            : [];
    }
}

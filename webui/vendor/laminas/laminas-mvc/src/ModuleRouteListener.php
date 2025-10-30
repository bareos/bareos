<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc;

use Laminas\EventManager\AbstractListenerAggregate;
use Laminas\EventManager\EventManagerInterface;

class ModuleRouteListener extends AbstractListenerAggregate
{
    const MODULE_NAMESPACE    = '__NAMESPACE__';
    const ORIGINAL_CONTROLLER = '__CONTROLLER__';

    /**
     * Attach to an event manager
     *
     * @param  EventManagerInterface $events
     * @param  int $priority
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        $this->listeners[] = $events->attach(MvcEvent::EVENT_ROUTE, [$this, 'onRoute'], $priority);
    }

    /**
     * Listen to the "route" event and determine if the module namespace should
     * be prepended to the controller name.
     *
     * If the route match contains a parameter key matching the MODULE_NAMESPACE
     * constant, that value will be prepended, with a namespace separator, to
     * the matched controller parameter.
     *
     * @param  MvcEvent $e
     * @return null
     */
    public function onRoute(MvcEvent $e)
    {
        $matches = $e->getRouteMatch();
        if (!$matches instanceof Router\RouteMatch) {
            // Can't do anything without a route match
            return;
        }

        $module = $matches->getParam(self::MODULE_NAMESPACE, false);
        if (!$module) {
            // No module namespace found; nothing to do
            return;
        }

        $controller = $matches->getParam('controller', false);
        if (!$controller) {
            // no controller matched, nothing to do
            return;
        }

        // Ensure the module namespace has not already been applied
        if (0 === strpos($controller, $module)) {
            return;
        }

        // Keep the originally matched controller name around
        $matches->setParam(self::ORIGINAL_CONTROLLER, $controller);

        // Prepend the controllername with the module, and replace it in the
        // matches
        $controller = $module . '\\' . str_replace(' ', '', ucwords(str_replace('-', ' ', $controller)));
        $matches->setParam('controller', $controller);
    }
}

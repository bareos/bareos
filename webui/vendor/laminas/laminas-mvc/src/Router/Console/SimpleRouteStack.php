<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Router\Console;

use Laminas\Mvc\Router\Exception;
use Laminas\Mvc\Router\SimpleRouteStack as BaseSimpleRouteStack;
use Laminas\Stdlib\ArrayUtils;
use Traversable;

/**
 * Tree search implementation.
 */
class SimpleRouteStack extends BaseSimpleRouteStack
{
    /**
     * init(): defined by SimpleRouteStack.
     *
     * @see    BaseSimpleRouteStack::init()
     */
    protected function init()
    {
        $routes = $this->routePluginManager;
        foreach ([
                'catchall' => __NAMESPACE__ . '\Catchall',
                'simple'   => __NAMESPACE__ . '\Simple',
            ] as $name => $class
        ) {
            $routes->setInvokableClass($name, $class);
        };
    }

    /**
     * addRoute(): defined by RouteStackInterface interface.
     *
     * @see    RouteStackInterface::addRoute()
     * @param  string  $name
     * @param  mixed   $route
     * @param  int $priority
     * @return SimpleRouteStack
     */
    public function addRoute($name, $route, $priority = null)
    {
        if (!$route instanceof RouteInterface) {
            $route = $this->routeFromArray($route);
        }

        return parent::addRoute($name, $route, $priority);
    }

    /**
     * routeFromArray(): defined by SimpleRouteStack.
     *
     * @see    BaseSimpleRouteStack::routeFromArray()
     * @param  array|Traversable $specs
     * @return RouteInterface
     * @throws Exception\InvalidArgumentException
     * @throws Exception\RuntimeException
     */
    protected function routeFromArray($specs)
    {
        if ($specs instanceof Traversable) {
            $specs = ArrayUtils::iteratorToArray($specs);
        } elseif (!is_array($specs)) {
            throw new Exception\InvalidArgumentException('Route definition must be an array or Traversable object');
        }

        // default to 'simple' console route
        if (!isset($specs['type'])) {
            $specs['type'] = 'simple';
        }

        // build route object
        $route = parent::routeFromArray($specs);

        if (!$route instanceof RouteInterface) {
            throw new Exception\RuntimeException('Given route does not implement Console route interface');
        }

        return $route;
    }
}

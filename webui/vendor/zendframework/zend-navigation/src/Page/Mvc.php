<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Navigation\Page;

use Zend\Mvc\Router as MvcRouter;
use Zend\Navigation\Exception;
use Zend\Router\RouteMatch;
use Zend\Router\RouteStackInterface;

/**
 * Represents a page that is defined using controller, action, route
 * name and route params to assemble the href
 *
 * The two constants defined were originally provided via the zend-mvc class
 * ModuleRouteListener; to remove the requirement on that component, they are
 * reproduced here.
 */
class Mvc extends AbstractPage
{
    const MODULE_NAMESPACE = '__NAMESPACE__';
    const ORIGINAL_CONTROLLER = '__CONTROLLER__';

    /**
     * Action name to use when assembling URL
     *
     * @var string
     */
    protected $action;

    /**
     * Controller name to use when assembling URL
     *
     * @var string
     */
    protected $controller;

    /**
     * URL query part to use when assembling URL
     *
     * @var array|string
     */
    protected $query;

    /**
     * Params to use when assembling URL
     *
     * @see getHref()
     * @var array
     */
    protected $params = [];

    /**
     * RouteInterface name to use when assembling URL
     *
     * @see getHref()
     * @var string
     */
    protected $route;

    /**
     * Cached href
     *
     * The use of this variable minimizes execution time when getHref() is
     * called more than once during the lifetime of a request. If a property
     * is updated, the cache is invalidated.
     *
     * @var string
     */
    protected $hrefCache;

    /**
     * RouteInterface matches; used for routing parameters and testing validity
     *
     * @var RouteMatch
     */
    protected $routeMatch;

    /**
     * If true and set routeMatch than getHref will use routeMatch params
     * to assemble uri
     * @var bool
     */
    protected $useRouteMatch = false;

    /**
     * Router for assembling URLs
     *
     * @see getHref()
     * @var RouteStackInterface
     */
    protected $router = null;

    /**
     * Default router to be used if router is not given.
     *
     * @see getHref()
     *
     * @var RouteStackInterface
     */
    protected static $defaultRouter = null;

    /**
     * Default route name
     *
     * @var string
     */
    protected static $defaultRoute = null;

    // Accessors:

    /**
     * Returns whether page should be considered active or not
     *
     * This method will compare the page properties against the route matches
     * composed in the object.
     *
     * @param  bool $recursive  [optional] whether page should be considered
     *                          active if any child pages are active. Default is
     *                          false.
     * @return bool             whether page should be considered active or not
     */
    public function isActive($recursive = false)
    {
        if (! $this->active) {
            $reqParams = [];
            if ($this->routeMatch instanceof RouteMatch || $this->routeMatch instanceof MvcRouter\RouteMatch) {
                $reqParams  = $this->routeMatch->getParams();

                if (isset($reqParams[self::ORIGINAL_CONTROLLER])) {
                    $reqParams['controller'] = $reqParams[self::ORIGINAL_CONTROLLER];
                }

                $pageParams   = $this->params;
                if (null !== $this->controller) {
                    $pageParams['controller'] = $this->controller;
                }
                if (null !== $this->action) {
                    $pageParams['action'] = $this->action;
                }

                if (null !== $this->getRoute()) {
                    if ($this->routeMatch->getMatchedRouteName() === $this->getRoute()
                        && (count(array_intersect_assoc($reqParams, $pageParams)) == count($pageParams))
                    ) {
                        $this->active = true;
                        return $this->active;
                    } else {
                        return parent::isActive($recursive);
                    }
                }
            }

            $pageParams = $this->params;

            if (null !== $this->controller) {
                $pageParams['controller'] = $this->controller;
            } else {
                /**
                 * @todo In ZF1, this was configurable and pulled from the front controller
                 */
                $pageParams['controller'] = 'index';
            }

            if (null !== $this->action) {
                $pageParams['action'] = $this->action;
            } else {
                /**
                 * @todo In ZF1, this was configurable and pulled from the front controller
                 */
                $pageParams['action'] = 'index';
            }

            if (count(array_intersect_assoc($reqParams, $pageParams)) == count($pageParams)) {
                $this->active = true;
                return true;
            }
        }

        return parent::isActive($recursive);
    }

    /**
     * Returns href for this page
     *
     * This method uses {@link RouteStackInterface} to assemble
     * the href based on the page's properties.
     *
     * @see RouteStackInterface
     * @return string  page href
     * @throws Exception\DomainException if no router is set
     */
    public function getHref()
    {
        if ($this->hrefCache) {
            return $this->hrefCache;
        }

        $router = $this->router;
        if (null === $router) {
            $router = static::$defaultRouter;
        }

        if (! $router instanceof RouteStackInterface && ! $router instanceof MvcRouter\RouteStackInterface) {
            throw new Exception\DomainException(
                __METHOD__
                . ' cannot execute as no Zend\Router\RouteStackInterface instance is composed'
            );
        }

        if ($this->useRouteMatch() && $this->getRouteMatch()) {
            $rmParams = $this->getRouteMatch()->getParams();

            if (isset($rmParams[self::ORIGINAL_CONTROLLER])) {
                $rmParams['controller'] = $rmParams[self::ORIGINAL_CONTROLLER];
                unset($rmParams[self::ORIGINAL_CONTROLLER]);
            }

            if (isset($rmParams[self::MODULE_NAMESPACE])) {
                unset($rmParams[self::MODULE_NAMESPACE]);
            }

            $params = array_merge($rmParams, $this->getParams());
        } else {
            $params = $this->getParams();
        }


        if (($param = $this->getController()) !== null) {
            $params['controller'] = $param;
        }

        if (($param = $this->getAction()) !== null) {
            $params['action'] = $param;
        }

        switch (true) {
            case ($this->getRoute() !== null || static::getDefaultRoute() !== null):
                $name = ($this->getRoute() !== null) ? $this->getRoute() : static::getDefaultRoute();
                break;
            case ($this->getRouteMatch() !== null):
                $name = $this->getRouteMatch()->getMatchedRouteName();
                break;
            default:
                throw new Exception\DomainException('No route name could be found');
        }

        $options = ['name' => $name];

        // Add the fragment identifier if it is set
        $fragment = $this->getFragment();
        if (null !== $fragment) {
            $options['fragment'] = $fragment;
        }

        if (null !== ($query = $this->getQuery())) {
            $options['query'] = $query;
        }

        $url = $router->assemble($params, $options);

        return $this->hrefCache = $url;
    }

    /**
     * Sets action name to use when assembling URL
     *
     * @see getHref()
     *
     * @param  string $action             action name
     * @return Mvc   fluent interface, returns self
     * @throws Exception\InvalidArgumentException  if invalid $action is given
     */
    public function setAction($action)
    {
        if (null !== $action && ! is_string($action)) {
            throw new Exception\InvalidArgumentException(
                'Invalid argument: $action must be a string or null'
            );
        }

        $this->action    = $action;
        $this->hrefCache = null;
        return $this;
    }

    /**
     * Returns action name to use when assembling URL
     *
     * @see getHref()
     *
     * @return string|null  action name
     */
    public function getAction()
    {
        return $this->action;
    }

    /**
     * Sets controller name to use when assembling URL
     *
     * @see getHref()
     *
     * @param  string|null $controller    controller name
     * @return Mvc   fluent interface, returns self
     * @throws Exception\InvalidArgumentException  if invalid controller name is given
     */
    public function setController($controller)
    {
        if (null !== $controller && ! is_string($controller)) {
            throw new Exception\InvalidArgumentException(
                'Invalid argument: $controller must be a string or null'
            );
        }

        $this->controller = $controller;
        $this->hrefCache  = null;
        return $this;
    }

    /**
     * Returns controller name to use when assembling URL
     *
     * @see getHref()
     *
     * @return string|null  controller name or null
     */
    public function getController()
    {
        return $this->controller;
    }

    /**
     * Sets URL query part to use when assembling URL
     *
     * @see getHref()
     * @param  array|string|null $query    URL query part
     * @return self   fluent interface, returns self
     */
    public function setQuery($query)
    {
        $this->query      = $query;
        $this->hrefCache  = null;
        return $this;
    }

    /**
     * Returns URL query part to use when assembling URL
     *
     * @see getHref()
     *
     * @return array|string|null  URL query part (as an array or string) or null
     */
    public function getQuery()
    {
        return $this->query;
    }

    /**
     * Sets params to use when assembling URL
     *
     * @see getHref()
     * @param  array|null $params [optional] page params. Default is null
     *                            which sets no params.
     * @return Mvc  fluent interface, returns self
     */
    public function setParams(array $params = null)
    {
        $this->params = empty($params) ? [] : $params;
        $this->hrefCache = null;
        return $this;
    }

    /**
     * Returns params to use when assembling URL
     *
     * @see getHref()
     *
     * @return array  page params
     */
    public function getParams()
    {
        return $this->params;
    }

    /**
     * Sets route name to use when assembling URL
     *
     * @see getHref()
     *
     * @param  string $route Route name to use when assembling URL.
     * @return Mvc Fluent interface, returns self.
     * @throws Exception\InvalidArgumentException If invalid $route is given.
     */
    public function setRoute($route)
    {
        if (null !== $route && (! is_string($route) || strlen($route) < 1)) {
            throw new Exception\InvalidArgumentException(
                'Invalid argument: $route must be a non-empty string or null'
            );
        }

        $this->route     = $route;
        $this->hrefCache = null;
        return $this;
    }

    /**
     * Returns route name to use when assembling URL
     *
     * @see getHref()
     *
     * @return string  route name
     */
    public function getRoute()
    {
        return $this->route;
    }

    /**
     * Get the route match.
     *
     * @return RouteMatch
     */
    public function getRouteMatch()
    {
        return $this->routeMatch;
    }

    /**
     * Set route match object from which parameters will be retrieved
     *
     * @param  RouteMatch|MvcRouter\RouteMatch $matches
     * @return Mvc fluent interface, returns self
     */
    public function setRouteMatch($matches)
    {
        if (! $matches instanceof RouteMatch && ! $matches instanceof MvcRouter\RouteMatch) {
            throw new Exception\InvalidArgumentException(sprintf(
                'RouteMatch passed to %s must be either a %s or a %s instance; received %s',
                __METHOD__,
                RouteMatch::class,
                MvcRouter\RouteMatch::class,
                (is_object($router) ? get_class($router) : gettype($router))
            ));
        }
        $this->routeMatch = $matches;
        return $this;
    }

    /**
     * Get the useRouteMatch flag
     *
     * @return bool
     */
    public function useRouteMatch()
    {
        return $this->useRouteMatch;
    }

    /**
     * Set whether the page should use route match params for assembling link uri
     *
     * @see getHref()
     * @param bool $useRouteMatch [optional]
     * @return Mvc
     */
    public function setUseRouteMatch($useRouteMatch = true)
    {
        $this->useRouteMatch = (bool) $useRouteMatch;
        $this->hrefCache = null;
        return $this;
    }

    /**
     * Get the router.
     *
     * @return null|RouteStackInterface|MvcRouter\RouteStackInterface
     */
    public function getRouter()
    {
        return $this->router;
    }

    /**
     * Sets router for assembling URLs
     *
     * @see getHref()
     *
     * @param  RouteStackInterface|MvcRouter\RouteStackInterface $router Router
     * @return Mvc Fluent interface, returns self
     */
    public function setRouter($router)
    {
        if (! $router instanceof RouteStackInterface && ! $router instanceof MvcRouter\RouteStackInterface) {
            throw new Exception\InvalidArgumentException(sprintf(
                'Router passed to %s must be either a %s or a %s instance; received %s',
                __METHOD__,
                RouteStackInterface::class,
                MvcRouter\RouteStackInterface::class,
                (is_object($router) ? get_class($router) : gettype($router))
            ));
        }
        $this->router = $router;
        return $this;
    }

    /**
     * Sets the default router for assembling URLs.
     *
     * @see getHref()
     * @param  RouteStackInterface $router Router
     * @return void
     */
    public static function setDefaultRouter($router)
    {
        static::$defaultRouter = $router;
    }

    /**
     * Gets the default router for assembling URLs.
     *
     * @return RouteStackInterface
     */
    public static function getDefaultRouter()
    {
        return static::$defaultRouter;
    }

    /**
     * Set default route name
     *
     * @param string $route
     * @return void
     */
    public static function setDefaultRoute($route)
    {
        static::$defaultRoute = $route;
    }

    /**
     * Get default route name
     *
     * @return string
     */
    public static function getDefaultRoute()
    {
        return static::$defaultRoute;
    }

    // Public methods:

    /**
     * Returns an array representation of the page
     *
     * @return array  associative array containing all page properties
     */
    public function toArray()
    {
        return array_merge(
            parent::toArray(),
            [
                 'action'     => $this->getAction(),
                 'controller' => $this->getController(),
                 'params'     => $this->getParams(),
                 'route'      => $this->getRoute(),
                 'router'     => $this->getRouter(),
                 'route_match' => $this->getRouteMatch(),
            ]
        );
    }
}

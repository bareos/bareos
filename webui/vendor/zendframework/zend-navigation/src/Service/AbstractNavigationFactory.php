<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Navigation\Service;

use Interop\Container\ContainerInterface;
use Zend\Config;
use Zend\Http\Request;
use Zend\Mvc\Router\RouteMatch;
use Zend\Mvc\Router\RouteStackInterface as Router;
use Zend\Navigation\Exception;
use Zend\Navigation\Navigation;
use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

/**
 * Abstract navigation factory
 */
abstract class AbstractNavigationFactory implements FactoryInterface
{
    /**
     * @var array
     */
    protected $pages;

    /**
     * Create and return a new Navigation instance (v3).
     *
     * @param ContainerInterface $container
     * @param string $requestedName
     * @param null|array $options
     * @return Navigation
     */
    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        return new Navigation($this->getPages($container));
    }

    /**
     * Create and return a new Navigation instance (v2).
     *
     * @param ContainerInterface $container
     * @param null|string $name
     * @param null|string $requestedName
     * @return Navigation
     */
    public function createService(ServiceLocatorInterface $container, $name = null, $requestedName = null)
    {
        return $this($container, $requestedName);
    }

    /**
     * @abstract
     * @return string
     */
    abstract protected function getName();

    /**
     * @param ContainerInterface $container
     * @return array
     * @throws \Zend\Navigation\Exception\InvalidArgumentException
     */
    protected function getPages(ContainerInterface $container)
    {
        if (null === $this->pages) {
            $configuration = $container->get('config');

            if (!isset($configuration['navigation'])) {
                throw new Exception\InvalidArgumentException('Could not find navigation configuration key');
            }
            if (!isset($configuration['navigation'][$this->getName()])) {
                throw new Exception\InvalidArgumentException(sprintf(
                    'Failed to find a navigation container by the name "%s"',
                    $this->getName()
                ));
            }

            $pages       = $this->getPagesFromConfig($configuration['navigation'][$this->getName()]);
            $this->pages = $this->preparePages($container, $pages);
        }
        return $this->pages;
    }

    /**
     * @param ContainerInterface $container
     * @param array|\Zend\Config\Config $pages
     * @return null|array
     * @throws \Zend\Navigation\Exception\InvalidArgumentException
     */
    protected function preparePages(ContainerInterface $container, $pages)
    {
        $application = $container->get('Application');
        $routeMatch  = $application->getMvcEvent()->getRouteMatch();
        $router      = $application->getMvcEvent()->getRouter();
        $request     = $application->getMvcEvent()->getRequest();

        // HTTP request is the only one that may be injected
        if (!$request instanceof Request) {
            $request = null;
        }

        return $this->injectComponents($pages, $routeMatch, $router, $request);
    }

    /**
     * @param string|\Zend\Config\Config|array $config
     * @return array|null|\Zend\Config\Config
     * @throws \Zend\Navigation\Exception\InvalidArgumentException
     */
    protected function getPagesFromConfig($config = null)
    {
        if (is_string($config)) {
            if (file_exists($config)) {
                $config = Config\Factory::fromFile($config);
            } else {
                throw new Exception\InvalidArgumentException(sprintf(
                    'Config was a string but file "%s" does not exist',
                    $config
                ));
            }
        } elseif ($config instanceof Config\Config) {
            $config = $config->toArray();
        } elseif (!is_array($config)) {
            throw new Exception\InvalidArgumentException(
                'Invalid input, expected array, filename, or Zend\Config object'
            );
        }

        return $config;
    }

    /**
     * @param array $pages
     * @param RouteMatch $routeMatch
     * @param Router $router
     * @param null|Request $request
     * @return array
     */
    protected function injectComponents(
        array $pages,
        RouteMatch $routeMatch = null,
        Router $router = null,
        $request = null
    ) {
        foreach ($pages as &$page) {
            $hasUri = isset($page['uri']);
            $hasMvc = isset($page['action']) || isset($page['controller']) || isset($page['route']);
            if ($hasMvc) {
                if (!isset($page['routeMatch']) && $routeMatch) {
                    $page['routeMatch'] = $routeMatch;
                }
                if (!isset($page['router'])) {
                    $page['router'] = $router;
                }
            } elseif ($hasUri) {
                if (!isset($page['request'])) {
                    $page['request'] = $request;
                }
            }

            if (isset($page['pages'])) {
                $page['pages'] = $this->injectComponents($page['pages'], $routeMatch, $router, $request);
            }
        }
        return $pages;
    }
}

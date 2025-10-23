<?php

/**
 * @see       https://github.com/laminas/laminas-navigation for the canonical source repository
 * @copyright https://github.com/laminas/laminas-navigation/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-navigation/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Navigation\View;

use Interop\Container\ContainerInterface;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\View\Helper\Navigation as NavigationHelper;
use ReflectionProperty;

class NavigationHelperFactory implements FactoryInterface
{
    /**
     * Create and return a navigation helper instance. (v3)
     *
     * @param ContainerInterface $container
     * @param string $requestedName
     * @param null|array $options
     * @return NavigationHelper
     */
    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        $helper = new NavigationHelper();
        $helper->setServiceLocator($this->getApplicationServicesFromContainer($container));
        return $helper;
    }

    /**
     * Create and return a navigation helper instance. (v2)
     *
     * @param ServiceLocatorInterface $container
     * @param null|string $name
     * @param string $requestedName
     * @return NavigationHelper
     */
    public function createService(
        ServiceLocatorInterface $container,
        $name = null,
        $requestedName = NavigationHelper::class
    ) {
        return $this($container, $requestedName);
    }

    /**
     * Retrieve the application (parent) services from the container, if possible.
     *
     * @param ContainerInterface $container
     * @return ContainerInterface
     */
    private function getApplicationServicesFromContainer(ContainerInterface $container)
    {
        // v3
        if (method_exists($container, 'configure')) {
            $r = new ReflectionProperty($container, 'creationContext');
            $r->setAccessible(true);
            return $r->getValue($container) ?: $container;
        }

        // v2
        return $container->getServiceLocator() ?: $container;
    }
}

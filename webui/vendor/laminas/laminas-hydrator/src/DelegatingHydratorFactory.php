<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator;

use Interop\Container\ContainerInterface;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DelegatingHydratorFactory implements FactoryInterface
{
    /**
     * Creates DelegatingHydrator (v2)
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return DelegatingHydrator
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        return $this($serviceLocator, '');
    }

    /**
     * Creates DelegatingHydrator (v3)
     *
     * @param ContainerInterface $container
     * @param string $requestedName
     * @param array|null $options
     * @return DelegatingHydrator
     */
    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        $container = $this->marshalHydratorPluginManager($container);
        return new DelegatingHydrator($container);
    }

    /**
     * Locate and return a HydratorPluginManager instance.
     *
     * @param ContainerInterface $container
     * @return HydratorPluginManager
     */
    private function marshalHydratorPluginManager(ContainerInterface $container)
    {
        // Already one? Return it.
        if ($container instanceof HydratorPluginManager) {
            return $container;
        }

        // As typically registered with v3 (FQCN)
        if ($container->has(HydratorPluginManager::class)) {
            return $container->get(HydratorPluginManager::class);
        }

        if ($container->has(\Zend\Hydrator\HydratorPluginManager::class)) {
            return $container->get(\Zend\Hydrator\HydratorPluginManager::class);
        }

        // As registered by laminas-mvc
        if ($container->has('HydratorManager')) {
            return $container->get('HydratorManager');
        }

        // Fallback: create one
        return new HydratorPluginManager($container);
    }
}

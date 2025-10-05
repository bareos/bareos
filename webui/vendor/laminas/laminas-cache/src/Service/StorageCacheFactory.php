<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Service;

use Interop\Container\ContainerInterface;
use Laminas\Cache\Storage\StorageInterface;
use Laminas\Cache\StorageFactory;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

/**
 * Storage cache factory.
 */
class StorageCacheFactory implements FactoryInterface
{
    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        $config = $container->get('config');
        $cacheConfig = isset($config['cache']) ? $config['cache'] : [];
        return StorageFactory::factory($cacheConfig);
    }

    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        return $this($serviceLocator, StorageInterface::class);
    }
}

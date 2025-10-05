<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Proxy;

use Laminas\ServiceManager\Exception;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use ProxyManager\Configuration;
use ProxyManager\Factory\LazyLoadingValueHolderFactory;
use ProxyManager\GeneratorStrategy\EvaluatingGeneratorStrategy;

/**
 * Service factory responsible of instantiating {@see \Laminas\ServiceManager\Proxy\LazyServiceFactory}
 * and configuring it starting from application configuration
 */
class LazyServiceFactoryFactory implements FactoryInterface
{
    /**
     * {@inheritDoc}
     *
     * @return \Laminas\ServiceManager\Proxy\LazyServiceFactory
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $config = $serviceLocator->get('Config');

        if (!isset($config['lazy_services'])) {
            throw new Exception\InvalidArgumentException('Missing "lazy_services" config key');
        }

        $lazyServices = $config['lazy_services'];

        if (!isset($lazyServices['class_map'])) {
            throw new Exception\InvalidArgumentException('Missing "class_map" config key in "lazy_services"');
        }

        $factoryConfig = new Configuration();

        if (isset($lazyServices['proxies_namespace'])) {
            $factoryConfig->setProxiesNamespace($lazyServices['proxies_namespace']);
        }

        if (isset($lazyServices['proxies_target_dir'])) {
            $factoryConfig->setProxiesTargetDir($lazyServices['proxies_target_dir']);
        }

        if (!isset($lazyServices['write_proxy_files']) || ! $lazyServices['write_proxy_files']) {
            $factoryConfig->setGeneratorStrategy(new EvaluatingGeneratorStrategy());
        }

        spl_autoload_register($factoryConfig->getProxyAutoloader());

        return new LazyServiceFactory(new LazyLoadingValueHolderFactory($factoryConfig), $lazyServices['class_map']);
    }
}

<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Proxy;

use Laminas\ServiceManager\DelegatorFactoryInterface;
use Laminas\ServiceManager\Exception;
use Laminas\ServiceManager\ServiceLocatorInterface;
use ProxyManager\Factory\LazyLoadingValueHolderFactory;
use ProxyManager\Proxy\LazyLoadingInterface;

/**
 * Delegator factory responsible of instantiating lazy loading value holder proxies of
 * given services at runtime
 *
 * @link https://github.com/Ocramius/ProxyManager/blob/master/docs/lazy-loading-value-holder.md
 */
class LazyServiceFactory implements DelegatorFactoryInterface
{
    /**
     * @var \ProxyManager\Factory\LazyLoadingValueHolderFactory
     */
    protected $proxyFactory;

    /**
     * @var string[] map of service names to class names
     */
    protected $servicesMap;

    /**
     * @param LazyLoadingValueHolderFactory $proxyFactory
     * @param string[]                      $servicesMap  a map of service names to class names of their
     *                                                    respective classes
     */
    public function __construct(LazyLoadingValueHolderFactory $proxyFactory, array $servicesMap)
    {
        $this->proxyFactory = $proxyFactory;
        $this->servicesMap  = $servicesMap;
    }

    /**
     * {@inheritDoc}
     *
     * @return object|\ProxyManager\Proxy\LazyLoadingInterface|\ProxyManager\Proxy\ValueHolderInterface
     */
    public function createDelegatorWithName(ServiceLocatorInterface $serviceLocator, $name, $requestedName, $callback)
    {
        $initializer = function (& $wrappedInstance, LazyLoadingInterface $proxy) use ($callback) {
            $proxy->setProxyInitializer(null);

            $wrappedInstance = call_user_func($callback);

            return true;
        };

        if (isset($this->servicesMap[$requestedName])) {
            return $this->proxyFactory->createProxy($this->servicesMap[$requestedName], $initializer);
        } elseif (isset($this->servicesMap[$name])) {
            return $this->proxyFactory->createProxy($this->servicesMap[$name], $initializer);
        }

        throw new Exception\InvalidServiceNameException(
            sprintf('The requested service "%s" was not found in the provided services map', $requestedName)
        );
    }
}

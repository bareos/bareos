<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\Di\Di;
use Laminas\Di\Exception\ClassNotFoundException;
use Laminas\Mvc\Exception\DomainException;
use Laminas\ServiceManager\AbstractFactoryInterface;
use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\Exception;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiStrictAbstractServiceFactory extends Di implements AbstractFactoryInterface
{
    /**@#+
     * constants
     */
    const USE_SL_BEFORE_DI = 'before';
    const USE_SL_AFTER_DI  = 'after';
    const USE_SL_NONE      = 'none';
    /**@#-*/

    /**
     * @var Di
     */
    protected $di = null;

    /**
     * @var string
     */
    protected $useServiceLocator = self::USE_SL_AFTER_DI;

    /**
     * @var ServiceLocatorInterface
     */
    protected $serviceLocator = null;

    /**
     * @var array an array of whitelisted service names (keys are the service names)
     */
    protected $allowedServiceNames = [];

    /**
     * @param Di $di
     * @param string $useServiceLocator
     */
    public function __construct(Di $di, $useServiceLocator = self::USE_SL_NONE)
    {
        $this->useServiceLocator = $useServiceLocator;
        // since we are using this in a proxy-fashion, localize state
        $this->di              = $di;
        $this->definitions     = $this->di->definitions;
        $this->instanceManager = $this->di->instanceManager;
    }

    /**
     * @param array $allowedServiceNames
     */
    public function setAllowedServiceNames(array $allowedServiceNames)
    {
        $this->allowedServiceNames = array_flip(array_values($allowedServiceNames));
    }

    /**
     * @return array
     */
    public function getAllowedServiceNames()
    {
        return array_keys($this->allowedServiceNames);
    }

    /**
     * {@inheritDoc}
     *
     * Allows creation of services only when in a whitelist
     */
    public function createServiceWithName(ServiceLocatorInterface $serviceLocator, $serviceName, $requestedName)
    {
        if (!isset($this->allowedServiceNames[$requestedName])) {
            throw new Exception\InvalidServiceNameException('Service "' . $requestedName . '" is not whitelisted');
        }

        if ($serviceLocator instanceof AbstractPluginManager) {
            /* @var $serviceLocator AbstractPluginManager */
            $this->serviceLocator = $serviceLocator->getServiceLocator();
        } else {
            $this->serviceLocator = $serviceLocator;
        }

        return parent::get($requestedName);
    }

    /**
     * Overrides Laminas\Di to allow the given serviceLocator's services to be reused by Di itself
     *
     * {@inheritDoc}
     *
     * @throws Exception\InvalidServiceNameException
     */
    public function get($name, array $params = [])
    {
        if (null === $this->serviceLocator) {
            throw new DomainException('No ServiceLocator defined, use `createServiceWithName` instead of `get`');
        }

        if (self::USE_SL_BEFORE_DI === $this->useServiceLocator && $this->serviceLocator->has($name)) {
            return $this->serviceLocator->get($name);
        }

        try {
            return parent::get($name, $params);
        } catch (ClassNotFoundException $e) {
            if (self::USE_SL_AFTER_DI === $this->useServiceLocator && $this->serviceLocator->has($name)) {
                return $this->serviceLocator->get($name);
            }

            throw new Exception\ServiceNotFoundException(
                sprintf('Service %s was not found in this DI instance', $name),
                null,
                $e
            );
        }
    }

    /**
     * {@inheritDoc}
     *
     * Allows creation of services only when in a whitelist
     */
    public function canCreateServiceWithName(ServiceLocatorInterface $serviceLocator, $name, $requestedName)
    {
        // won't check if the service exists, we are trusting the user's whitelist
        return isset($this->allowedServiceNames[$requestedName]);
    }
}

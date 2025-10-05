<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Di;

use Laminas\Di\Di;
use Laminas\Di\Exception\ClassNotFoundException as DiClassNotFoundException;
use Laminas\ServiceManager\Exception;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiServiceFactory extends Di implements FactoryInterface
{
    /**@#+
     * constants
     */
    const USE_SL_BEFORE_DI = 'before';
    const USE_SL_AFTER_DI  = 'after';
    const USE_SL_NONE      = 'none';
    /**@#-*/

    /**
     * @var \Laminas\Di\Di
     */
    protected $di = null;

    /**
     * @var \Laminas\Di\InstanceManager
     */
    protected $name = null;

    /**
     * @var array
     */
    protected $parameters = [];

    /**
     * @var string
     */
    protected $useServiceLocator = self::USE_SL_AFTER_DI;

    /**
     * @var ServiceLocatorInterface
     */
    protected $serviceLocator = null;

    /**
     * Constructor
     *
     * @param \Laminas\Di\Di $di
     * @param null|\Laminas\Di\InstanceManager $name
     * @param array $parameters
     * @param string $useServiceLocator
     */
    public function __construct(Di $di, $name, array $parameters = [], $useServiceLocator = self::USE_SL_NONE)
    {
        $this->di = $di;
        $this->name = $name;
        $this->parameters = $parameters;
        if (in_array($useServiceLocator, [self::USE_SL_BEFORE_DI, self::USE_SL_AFTER_DI, self::USE_SL_NONE])) {
            $this->useServiceLocator = $useServiceLocator;
        }

        // since we are using this in a proxy-fashion, localize state
        $this->definitions = $this->di->definitions;
        $this->instanceManager = $this->di->instanceManager;
    }

    /**
     * Create service
     *
     * @param ServiceLocatorInterface $serviceLocator
     * @return object
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $this->serviceLocator = $serviceLocator;
        return $this->get($this->name, $this->parameters);
    }

    /**
     * Override, as we want it to use the functionality defined in the proxy
     *
     * @param string $name
     * @param array $params
     * @return object
     * @throws Exception\ServiceNotFoundException
     */
    public function get($name, array $params = [])
    {
        // allow this di service to get dependencies from the service locator BEFORE trying di
        if ($this->useServiceLocator == self::USE_SL_BEFORE_DI && $this->serviceLocator->has($name)) {
            return $this->serviceLocator->get($name);
        }

        try {
            $service = parent::get($name, $params);
            return $service;
        } catch (DiClassNotFoundException $e) {
            // allow this di service to get dependencies from the service locator AFTER trying di
            if ($this->useServiceLocator == self::USE_SL_AFTER_DI && $this->serviceLocator->has($name)) {
                return $this->serviceLocator->get($name);
            } else {
                throw new Exception\ServiceNotFoundException(
                    sprintf('Service %s was not found in this DI instance', $name),
                    null,
                    $e
                );
            }
        }
    }
}

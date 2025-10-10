<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Di;

use Laminas\Di\Di;
use Laminas\ServiceManager\InitializerInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiServiceInitializer extends Di implements InitializerInterface
{
    /**
     * @var Di
     */
    protected $di = null;

    /**
     * @var DiInstanceManagerProxy
     */
    protected $diInstanceManagerProxy = null;

    /**
     * @var ServiceLocatorInterface
     */
    protected $serviceLocator = null;

    /**
     * Constructor
     *
     * @param \Laminas\Di\Di $di
     * @param \Laminas\ServiceManager\ServiceLocatorInterface $serviceLocator
     * @param null|DiInstanceManagerProxy $diImProxy
     */
    public function __construct(Di $di, ServiceLocatorInterface $serviceLocator, DiInstanceManagerProxy $diImProxy = null)
    {
        $this->di = $di;
        $this->serviceLocator = $serviceLocator;
        $this->diInstanceManagerProxy = ($diImProxy) ?: new DiInstanceManagerProxy($di->instanceManager(), $serviceLocator);
    }

    /**
     * Initialize
     *
     * @param $instance
     * @param ServiceLocatorInterface $serviceLocator
     * @throws \Exception
     */
    public function initialize($instance, ServiceLocatorInterface $serviceLocator)
    {
        $instanceManager = $this->di->instanceManager;
        $this->di->instanceManager = $this->diInstanceManagerProxy;
        try {
            $this->di->injectDependencies($instance);
            $this->di->instanceManager = $instanceManager;
        } catch (\Exception $e) {
            $this->di->instanceManager = $instanceManager;
            throw $e;
        }
    }
}

<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Di;

use Laminas\Di\InstanceManager as DiInstanceManager;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiInstanceManagerProxy extends DiInstanceManager
{
    /**
     * @var DiInstanceManager
     */
    protected $diInstanceManager = null;

    /**
     * @var ServiceLocatorInterface
     */
    protected $serviceLocator = null;

    /**
     * Constructor
     *
     * @param DiInstanceManager $diInstanceManager
     * @param ServiceLocatorInterface $serviceLocator
     */
    public function __construct(DiInstanceManager $diInstanceManager, ServiceLocatorInterface $serviceLocator)
    {
        $this->diInstanceManager = $diInstanceManager;
        $this->serviceLocator = $serviceLocator;

        // localize state
        $this->aliases = &$diInstanceManager->aliases;
        $this->sharedInstances = &$diInstanceManager->sharedInstances;
        $this->sharedInstancesWithParams = &$diInstanceManager->sharedInstancesWithParams;
        $this->configurations = &$diInstanceManager->configurations;
        $this->typePreferences = &$diInstanceManager->typePreferences;
    }

    /**
     * Determine if we have a shared instance by class or alias
     *
     * @param $classOrAlias
     * @return bool
     */
    public function hasSharedInstance($classOrAlias)
    {
        return ($this->serviceLocator->has($classOrAlias) || $this->diInstanceManager->hasSharedInstance($classOrAlias));
    }

    /**
     * Get shared instance
     *
     * @param $classOrAlias
     * @return mixed
     */
    public function getSharedInstance($classOrAlias)
    {
        if ($this->serviceLocator->has($classOrAlias)) {
            return $this->serviceLocator->get($classOrAlias);
        }

        return $this->diInstanceManager->getSharedInstance($classOrAlias);
    }
}

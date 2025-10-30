<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\ServiceManager\Di\DiAbstractServiceFactory;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\ServiceManager\ServiceManager;

class DiAbstractServiceFactoryFactory implements FactoryInterface
{
    /**
     * Class responsible for instantiating a DiAbstractServiceFactory
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return DiAbstractServiceFactory
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $factory = new DiAbstractServiceFactory($serviceLocator->get('Di'), DiAbstractServiceFactory::USE_SL_BEFORE_DI);

        if ($serviceLocator instanceof ServiceManager) {
            /* @var $serviceLocator ServiceManager */
            $serviceLocator->addAbstractFactory($factory, false);
        }

        return $factory;
    }
}

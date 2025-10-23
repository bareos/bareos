<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\ServiceManager\Di\DiServiceInitializer;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiServiceInitializerFactory implements FactoryInterface
{
    /**
     * Class responsible for instantiating a DiServiceInitializer
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return DiServiceInitializer
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        return new DiServiceInitializer($serviceLocator->get('Di'), $serviceLocator);
    }
}

<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class DiStrictAbstractServiceFactoryFactory implements FactoryInterface
{
    /**
     * Class responsible for instantiating a DiStrictAbstractServiceFactory
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return DiStrictAbstractServiceFactory
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $diAbstractFactory = new DiStrictAbstractServiceFactory(
            $serviceLocator->get('Di'),
            DiStrictAbstractServiceFactory::USE_SL_BEFORE_DI
        );
        $config = $serviceLocator->get('Config');

        if (isset($config['di']['allowed_controllers'])) {
            $diAbstractFactory->setAllowedServiceNames($config['di']['allowed_controllers']);
        }

        return $diAbstractFactory;
    }
}

<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\View\Strategy\JsonStrategy;

class ViewJsonStrategyFactory implements FactoryInterface
{
    /**
     * Create and return the JSON view strategy
     *
     * Retrieves the ViewJsonRenderer service from the service locator, and
     * injects it into the constructor for the JSON strategy.
     *
     * It then attaches the strategy to the View service, at a priority of 100.
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return JsonStrategy
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        $jsonRenderer = $serviceLocator->get('ViewJsonRenderer');
        $jsonStrategy = new JsonStrategy($jsonRenderer);
        return $jsonStrategy;
    }
}

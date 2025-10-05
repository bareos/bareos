<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\Console\Console;
use Laminas\Mvc\View\Console\ViewManager as ConsoleViewManager;
use Laminas\Mvc\View\Http\ViewManager as HttpViewManager;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class ViewManagerFactory implements FactoryInterface
{
    /**
     * Create and return a view manager based on detected environment
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return ConsoleViewManager|HttpViewManager
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        if (Console::isConsole()) {
            return $serviceLocator->get('ConsoleViewManager');
        }

        return $serviceLocator->get('HttpViewManager');
    }
}

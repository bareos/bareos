<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Service;

use Laminas\Console\Console;
use Laminas\Mvc\View\Console\ViewManager as ConsoleViewManager;
use Laminas\ServiceManager\Exception\ServiceNotCreatedException;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;

class ConsoleViewManagerFactory implements FactoryInterface
{
    /**
     * Create and return the view manager for the console environment
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return ConsoleViewManager
     */
    public function createService(ServiceLocatorInterface $serviceLocator)
    {
        if (!Console::isConsole()) {
            throw new ServiceNotCreatedException(
                'ConsoleViewManager requires a Console environment; console environment not detected'
            );
        }

        return new ConsoleViewManager();
    }
}

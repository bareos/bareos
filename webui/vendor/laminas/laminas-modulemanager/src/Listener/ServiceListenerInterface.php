<?php

/**
 * @see       https://github.com/laminas/laminas-modulemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-modulemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ModuleManager\Listener;

use Laminas\EventManager\ListenerAggregateInterface;
use Laminas\ServiceManager\ServiceManager;

interface ServiceListenerInterface extends ListenerAggregateInterface
{
    /**
     * @param  ServiceManager|string $serviceManager  Service Manager instance or name
     * @param  string                $key             Configuration key
     * @param  string                $moduleInterface FQCN as string
     * @param  string                $method          Method name
     * @return ServiceListenerInterface
     */
    public function addServiceManager($serviceManager, $key, $moduleInterface, $method);

    /**
     * @param  array $configuration
     * @return ServiceListenerInterface
     */
    public function setDefaultServiceConfig($configuration);
}

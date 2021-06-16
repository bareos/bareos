<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Zend\ModuleManager\Listener;

use Zend\EventManager\ListenerAggregateInterface;

interface ServiceListenerInterface extends ListenerAggregateInterface
{
    /**
     * Provide metadata describing how to aggregate service/plugin manager configuration.
     *
     * - $serviceManager is the service name for the service/plugin manager.
     * - $key is the configuration key containing configuration for it.
     * - $moduleInterface is the interface indicating a configuration provider for it.
     * - $method is used for duck-typing configuration providers.
     *
     * @param  string $serviceManager  Service name for service/plugin manager
     * @param  string $key             Configuration key
     * @param  string $moduleInterface FQCN as string
     * @param  string $method          Method name
     */
    public function addServiceManager($serviceManager, $key, $moduleInterface, $method);

    /**
     * @param  array $configuration
     * @return ServiceListenerInterface
     */
    public function setDefaultServiceConfig($configuration);
}

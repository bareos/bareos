<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session;

class ConfigProvider
{
    /**
     * Retrieve configuration for laminas-session.
     *
     * @return array
     */
    public function __invoke()
    {
        return [
            'dependencies' => $this->getDependencyConfig(),
        ];
    }

    /**
     * Retrieve dependency config for laminas-session.
     *
     * @return array
     */
    public function getDependencyConfig()
    {
        return [
            'abstract_factories' => [
                Service\ContainerAbstractServiceFactory::class,
            ],
            'aliases' => [
                SessionManager::class => ManagerInterface::class,

                // Legacy Zend Framework aliases
                \Zend\Session\SessionManager::class => SessionManager::class,
                \Zend\Session\Config\ConfigInterface::class => Config\ConfigInterface::class,
                \Zend\Session\ManagerInterface::class => ManagerInterface::class,
                \Zend\Session\Storage\StorageInterface::class => Storage\StorageInterface::class,
            ],
            'factories' => [
                Config\ConfigInterface::class => Service\SessionConfigFactory::class,
                ManagerInterface::class => Service\SessionManagerFactory::class,
                Storage\StorageInterface::class => Service\StorageFactory::class,
            ],
        ];
    }
}

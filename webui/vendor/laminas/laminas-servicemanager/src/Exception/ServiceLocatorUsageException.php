<?php

/**
 * @see       https://github.com/laminas/laminas-servicemanager for the canonical source repository
 * @copyright https://github.com/laminas/laminas-servicemanager/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-servicemanager/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\ServiceManager\Exception;

use Exception as BaseException;
use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\ServiceLocatorInterface;

class ServiceLocatorUsageException extends ServiceNotFoundException
{
    /**
     * Static constructor
     *
     * @param AbstractPluginManager   $pluginManager
     * @param ServiceLocatorInterface $parentLocator
     * @param string                  $serviceName
     * @param BaseException           $previousException
     *
     * @return self
     */
    public static function fromInvalidPluginManagerRequestedServiceName(
        AbstractPluginManager $pluginManager,
        ServiceLocatorInterface $parentLocator,
        $serviceName,
        BaseException $previousException
    ) {
        return new self(
            sprintf(
                "Service \"%s\" has been requested to plugin manager of type \"%s\", but couldn't be retrieved.\n"
                . "A previous exception of type \"%s\" has been raised in the process.\n"
                . "By the way, a service with the name \"%s\" has been found in the parent service locator \"%s\": "
                . 'did you forget to use $parentLocator = $serviceLocator->getServiceLocator() in your factory code?',
                $serviceName,
                get_class($pluginManager),
                get_class($previousException),
                $serviceName,
                get_class($parentLocator)
            ),
            0,
            $previousException
        );
    }
}

<?php

/**
 * @see       https://github.com/laminas/laminas-session for the canonical source repository
 * @copyright https://github.com/laminas/laminas-session/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-session/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Session\Service;

use Interop\Container\ContainerInterface;
use Laminas\ServiceManager\Exception\ServiceNotCreatedException;
use Laminas\ServiceManager\FactoryInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\Session\Config\ConfigInterface;
use Laminas\Session\Config\SessionConfig;

class SessionConfigFactory implements FactoryInterface
{
    /**
     * Create session configuration object (v3 usage).
     *
     * Uses "session_config" section of configuration to seed a ConfigInterface
     * instance. By default, Laminas\Session\Config\SessionConfig will be used, but
     * you may also specify a specific implementation variant using the
     * "config_class" subkey.
     *
     * @param ContainerInterface $container
     * @param string $requestedName
     * @param null|array $options
     * @return ConfigInterface
     * @throws ServiceNotCreatedException if session_config is missing, or an
     *     invalid config_class is used
     */
    public function __invoke(ContainerInterface $container, $requestedName, array $options = null)
    {
        $config = $container->get('config');
        if (! isset($config['session_config']) || ! is_array($config['session_config'])) {
            throw new ServiceNotCreatedException(
                'Configuration is missing a "session_config" key, or the value of that key is not an array'
            );
        }

        $class  = SessionConfig::class;
        $config = $config['session_config'];
        if (isset($config['config_class'])) {
            if (! class_exists($config['config_class'])) {
                throw new ServiceNotCreatedException(sprintf(
                    'Invalid configuration class "%s" specified in "config_class" session configuration; '
                    . 'must be a valid class',
                    $config['config_class']
                ));
            }
            $class = $config['config_class'];
            unset($config['config_class']);
        }

        $sessionConfig = new $class();
        if (! $sessionConfig instanceof ConfigInterface) {
            throw new ServiceNotCreatedException(sprintf(
                'Invalid configuration class "%s" specified in "config_class" session configuration; must implement %s',
                $class,
                ConfigInterface::class
            ));
        }
        $sessionConfig->setOptions($config);

        return $sessionConfig;
    }

    /**
     * Create and return a config instance (v2 usage).
     *
     * @param ServiceLocatorInterface $services
     * @param null|string $canonicalName
     * @param string $requestedName
     * @return ConfigInterface
     */
    public function createService(
        ServiceLocatorInterface $services,
        $canonicalName = null,
        $requestedName = ConfigInterface::class
    ) {
        return $this($services, $requestedName);
    }
}

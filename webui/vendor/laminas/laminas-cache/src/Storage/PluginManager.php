<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache\Storage;

use Laminas\Cache\Exception;
use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\Exception\InvalidServiceException;
use Laminas\ServiceManager\Factory\InvokableFactory;

/**
 * Plugin manager implementation for cache plugins
 *
 * Enforces that plugins retrieved are instances of
 * Plugin\PluginInterface. Additionally, it registers a number of default
 * plugins available.
 */
class PluginManager extends AbstractPluginManager
{
    protected $aliases = [
        'clearexpiredbyfactor' => Plugin\ClearExpiredByFactor::class,
        'clearExpiredByFactor' => Plugin\ClearExpiredByFactor::class,
        'ClearExpiredByFactor' => Plugin\ClearExpiredByFactor::class,
        'exceptionhandler'     => Plugin\ExceptionHandler::class,
        'exceptionHandler'     => Plugin\ExceptionHandler::class,
        'ExceptionHandler'     => Plugin\ExceptionHandler::class,
        'ignoreuserabort'      => Plugin\IgnoreUserAbort::class,
        'ignoreUserAbort'      => Plugin\IgnoreUserAbort::class,
        'IgnoreUserAbort'      => Plugin\IgnoreUserAbort::class,
        'optimizebyfactor'     => Plugin\OptimizeByFactor::class,
        'optimizeByFactor'     => Plugin\OptimizeByFactor::class,
        'OptimizeByFactor'     => Plugin\OptimizeByFactor::class,
        'serializer'           => Plugin\Serializer::class,
        'Serializer'           => Plugin\Serializer::class,

        // Legacy Zend Framework aliases
        \Zend\Cache\Storage\Plugin\ClearExpiredByFactor::class => Plugin\ClearExpiredByFactor::class,
        \Zend\Cache\Storage\Plugin\ExceptionHandler::class => Plugin\ExceptionHandler::class,
        \Zend\Cache\Storage\Plugin\IgnoreUserAbort::class => Plugin\IgnoreUserAbort::class,
        \Zend\Cache\Storage\Plugin\OptimizeByFactor::class => Plugin\OptimizeByFactor::class,
        \Zend\Cache\Storage\Plugin\Serializer::class => Plugin\Serializer::class,

        // v2 normalized FQCNs
        'zendcachestoragepluginclearexpiredbyfactor' => Plugin\ClearExpiredByFactor::class,
        'zendcachestoragepluginexceptionhandler' => Plugin\ExceptionHandler::class,
        'zendcachestoragepluginignoreuserabort' => Plugin\IgnoreUserAbort::class,
        'zendcachestoragepluginoptimizebyfactor' => Plugin\OptimizeByFactor::class,
        'zendcachestoragepluginserializer' => Plugin\Serializer::class,
    ];

    protected $factories = [
        Plugin\ClearExpiredByFactor::class           => InvokableFactory::class,
        Plugin\ExceptionHandler::class               => InvokableFactory::class,
        Plugin\IgnoreUserAbort::class                => InvokableFactory::class,
        Plugin\OptimizeByFactor::class               => InvokableFactory::class,
        Plugin\Serializer::class                     => InvokableFactory::class,

        // v2 normalized FQCNs
        'laminascachestoragepluginclearexpiredbyfactor' => InvokableFactory::class,
        'laminascachestoragepluginexceptionhandler'     => InvokableFactory::class,
        'laminascachestoragepluginignoreuserabort'      => InvokableFactory::class,
        'laminascachestoragepluginoptimizebyfactor'     => InvokableFactory::class,
        'laminascachestoragepluginserializer'           => InvokableFactory::class,
    ];

    /**
     * Do not share by default (v3)
     *
     * @var array
     */
    protected $sharedByDefault = false;

    /**
     * Do not share by default (v2)
     *
     * @var array
     */
    protected $shareByDefault = false;

    /**
     * @var string
     */
    protected $instanceOf = Plugin\PluginInterface::class;

    /**
     * {@inheritdoc}
     */
    public function validate($instance)
    {
        if ($instance instanceof $this->instanceOf) {
            // we're okay
            return;
        }

        throw new InvalidServiceException(sprintf(
            'Plugin of type %s is invalid; must implement %s\Plugin\PluginInterface',
            (is_object($instance) ? get_class($instance) : gettype($instance)),
            __NAMESPACE__
        ));
    }

    /**
     * Validate the plugin
     *
     * Checks that the plugin loaded is an instance of Plugin\PluginInterface.
     *
     * @param  mixed $plugin
     * @return void
     * @throws Exception\RuntimeException if invalid
     */
    public function validatePlugin($plugin)
    {
        try {
            $this->validate($plugin);
        } catch (InvalidServiceException $e) {
            throw new Exception\RuntimeException($e->getMessage(), $e->getCode(), $e);
        }
    }
}

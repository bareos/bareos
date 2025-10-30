<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache;

use Laminas\ServiceManager\AbstractPluginManager;
use Laminas\ServiceManager\Exception\InvalidServiceException;
use Laminas\ServiceManager\Factory\InvokableFactory;

/**
 * Plugin manager implementation for cache pattern adapters
 *
 * Enforces that retrieved adapters are instances of
 * Pattern\PatternInterface. Additionally, it registers a number of default
 * patterns available.
 */
class PatternPluginManager extends AbstractPluginManager
{
    protected $aliases = [
        'callback' => Pattern\CallbackCache::class,
        'Callback' => Pattern\CallbackCache::class,
        'capture'  => Pattern\CaptureCache::class,
        'Capture'  => Pattern\CaptureCache::class,
        'class'    => Pattern\ClassCache::class,
        'Class'    => Pattern\ClassCache::class,
        'object'   => Pattern\ObjectCache::class,
        'Object'   => Pattern\ObjectCache::class,
        'output'   => Pattern\OutputCache::class,
        'Output'   => Pattern\OutputCache::class,

        // Legacy Zend Framework aliases
        \Zend\Cache\Pattern\CallbackCache::class => Pattern\CallbackCache::class,
        \Zend\Cache\Pattern\CaptureCache::class => Pattern\CaptureCache::class,
        \Zend\Cache\Pattern\ClassCache::class => Pattern\ClassCache::class,
        \Zend\Cache\Pattern\ObjectCache::class => Pattern\ObjectCache::class,
        \Zend\Cache\Pattern\OutputCache::class => Pattern\OutputCache::class,

        // v2 normalized FQCNs
        'zendcachepatterncallbackcache' => Pattern\CallbackCache::class,
        'zendcachepatterncapturecache' => Pattern\CaptureCache::class,
        'zendcachepatternclasscache' => Pattern\ClassCache::class,
        'zendcachepatternobjectcache' => Pattern\ObjectCache::class,
        'zendcachepatternoutputcache' => Pattern\OutputCache::class,
    ];

    protected $factories = [
        Pattern\CallbackCache::class    => InvokableFactory::class,
        Pattern\CaptureCache::class     => InvokableFactory::class,
        Pattern\ClassCache::class       => InvokableFactory::class,
        Pattern\ObjectCache::class      => InvokableFactory::class,
        Pattern\OutputCache::class      => InvokableFactory::class,

        // v2 normalized FQCNs
        'laminascachepatterncallbackcache' => InvokableFactory::class,
        'laminascachepatterncapturecache'  => InvokableFactory::class,
        'laminascachepatternclasscache'    => InvokableFactory::class,
        'laminascachepatternobjectcache'   => InvokableFactory::class,
        'laminascachepatternoutputcache'   => InvokableFactory::class,
    ];

    /**
     * Don't share by default
     *
     * @var boolean
     */
    protected $shareByDefault = false;

    /**
     * Don't share by default
     *
     * @var boolean
     */
    protected $sharedByDefault = false;

    /**
     * @var string
     */
    protected $instanceOf = Pattern\PatternInterface::class;

    /**
     * Validate the plugin is of the expected type (v3).
     *
     * Validates against `$instanceOf`.
     *
     * @param mixed $instance
     * @throws InvalidServiceException
     */
    public function validate($instance)
    {
        if (! $instance instanceof $this->instanceOf) {
            throw new InvalidServiceException(sprintf(
                '%s can only create instances of %s; %s is invalid',
                get_class($this),
                $this->instanceOf,
                (is_object($instance) ? get_class($instance) : gettype($instance))
            ));
        }
    }

    /**
     * Validate the plugin is of the expected type (v2).
     *
     * Proxies to `validate()`.
     *
     * @param mixed $plugin
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

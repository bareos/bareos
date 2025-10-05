<?php

/**
 * @see       https://github.com/laminas/laminas-cache for the canonical source repository
 * @copyright https://github.com/laminas/laminas-cache/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-cache/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Cache;

use Laminas\ServiceManager\ServiceManager;
use Laminas\Stdlib\ArrayUtils;
use Traversable;

abstract class PatternFactory
{
    /**
     * The pattern manager
     *
     * @var null|PatternPluginManager
     */
    protected static $plugins = null;

    /**
     * Instantiate a cache pattern
     *
     * @param  string|Pattern\PatternInterface $patternName
     * @param  array|Traversable|Pattern\PatternOptions $options
     * @return Pattern\PatternInterface
     * @throws Exception\InvalidArgumentException
     */
    public static function factory($patternName, $options = [])
    {
        if ($options instanceof Traversable) {
            $options = ArrayUtils::iteratorToArray($options);
        }
        if (is_array($options)) {
            $options = new Pattern\PatternOptions($options);
        } elseif (!$options instanceof Pattern\PatternOptions) {
            throw new Exception\InvalidArgumentException(sprintf(
                '%s expects an array, Traversable object, or %s\Pattern\PatternOptions object; received "%s"',
                __METHOD__,
                __NAMESPACE__,
                (is_object($options) ? get_class($options) : gettype($options))
            ));
        }

        if ($patternName instanceof Pattern\PatternInterface) {
            $patternName->setOptions($options);
            return $patternName;
        }

        $pattern = static::getPluginManager()->get($patternName);
        $pattern->setOptions($options);
        return $pattern;
    }

    /**
     * Get the pattern plugin manager
     *
     * @return PatternPluginManager
     */
    public static function getPluginManager()
    {
        if (static::$plugins === null) {
            static::$plugins = new PatternPluginManager(new ServiceManager);
        }

        return static::$plugins;
    }

    /**
     * Set the pattern plugin manager
     *
     * @param  PatternPluginManager $plugins
     * @return void
     */
    public static function setPluginManager(PatternPluginManager $plugins)
    {
        static::$plugins = $plugins;
    }

    /**
     * Reset pattern plugin manager to default
     *
     * @return void
     */
    public static function resetPluginManager()
    {
        static::$plugins = null;
    }
}

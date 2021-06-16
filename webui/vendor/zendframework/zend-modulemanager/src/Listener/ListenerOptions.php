<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace Zend\ModuleManager\Listener;

use Traversable;
use Zend\Stdlib\AbstractOptions;

/**
 * Listener options
 */
class ListenerOptions extends AbstractOptions
{
    /**
     * @var array
     */
    protected $modulePaths = [];

    /**
     * @var array
     */
    protected $configGlobPaths = [];

    /**
     * @var array
     */
    protected $configStaticPaths = [];

    /**
     * @var array
     */
    protected $extraConfig = [];

    /**
     * @var bool
     */
    protected $configCacheEnabled = false;

    /**
     * @var string
     */
    protected $configCacheKey;

    /**
     * @var string|null
     */
    protected $cacheDir;

    /**
     * @var bool
     */
    protected $checkDependencies = true;

    /**
     * @var bool
     */
    protected $moduleMapCacheEnabled = false;

    /**
     * @var string
     */
    protected $moduleMapCacheKey;

    /**
     * @var bool
     */
    protected $useZendLoader = true;

    /**
     * Get an array of paths where modules reside
     *
     * @return array
     */
    public function getModulePaths()
    {
        return $this->modulePaths;
    }

    /**
     * Set an array of paths where modules reside
     *
     * @param  array|Traversable $modulePaths
     * @throws Exception\InvalidArgumentException
     * @return ListenerOptions Provides fluent interface
     */
    public function setModulePaths($modulePaths)
    {
        if (! is_array($modulePaths) && ! $modulePaths instanceof Traversable) {
            throw new Exception\InvalidArgumentException(
                sprintf(
                    'Argument passed to %s::%s() must be an array, '
                    . 'implement the Traversable interface, or be an '
                    . 'instance of Zend\Config\Config. %s given.',
                    __CLASS__,
                    __METHOD__,
                    gettype($modulePaths)
                )
            );
        }

        $this->modulePaths = $modulePaths;
        return $this;
    }

    /**
     * Get the glob patterns to load additional config files
     *
     * @return array
     */
    public function getConfigGlobPaths()
    {
        return $this->configGlobPaths;
    }

    /**
     * Get the static paths to load additional config files
     *
     * @return array
     */
    public function getConfigStaticPaths()
    {
        return $this->configStaticPaths;
    }

    /**
     * Set the glob patterns to use for loading additional config files
     *
     * @param array|Traversable $configGlobPaths
     * @throws Exception\InvalidArgumentException
     * @return ListenerOptions Provides fluent interface
     */
    public function setConfigGlobPaths($configGlobPaths)
    {
        if (! is_array($configGlobPaths) && ! $configGlobPaths instanceof Traversable) {
            throw new Exception\InvalidArgumentException(
                sprintf(
                    'Argument passed to %s::%s() must be an array, '
                    . 'implement the Traversable interface, or be an '
                    . 'instance of Zend\Config\Config. %s given.',
                    __CLASS__,
                    __METHOD__,
                    gettype($configGlobPaths)
                )
            );
        }

        $this->configGlobPaths = $configGlobPaths;
        return $this;
    }

    /**
     * Set the static paths to use for loading additional config files
     *
     * @param array|Traversable $configStaticPaths
     * @throws Exception\InvalidArgumentException
     * @return ListenerOptions Provides fluent interface
     */
    public function setConfigStaticPaths($configStaticPaths)
    {
        if (! is_array($configStaticPaths) && ! $configStaticPaths instanceof Traversable) {
            throw new Exception\InvalidArgumentException(
                sprintf(
                    'Argument passed to %s::%s() must be an array, '
                    . 'implement the Traversable interface, or be an '
                    . 'instance of Zend\Config\Config. %s given.',
                    __CLASS__,
                    __METHOD__,
                    gettype($configStaticPaths)
                )
            );
        }

        $this->configStaticPaths = $configStaticPaths;
        return $this;
    }

    /**
     * Get any extra config to merge in.
     *
     * @return array|Traversable
     */
    public function getExtraConfig()
    {
        return $this->extraConfig;
    }

    /**
     * Add some extra config array to the main config. This is mainly useful
     * for unit testing purposes.
     *
     * @param array|Traversable $extraConfig
     * @throws Exception\InvalidArgumentException
     * @return ListenerOptions Provides fluent interface
     */
    public function setExtraConfig($extraConfig)
    {
        if (! is_array($extraConfig) && ! $extraConfig instanceof Traversable) {
            throw new Exception\InvalidArgumentException(
                sprintf(
                    'Argument passed to %s::%s() must be an array, '
                    . 'implement the Traversable interface, or be an '
                    . 'instance of Zend\Config\Config. %s given.',
                    __CLASS__,
                    __METHOD__,
                    gettype($extraConfig)
                )
            );
        }

        $this->extraConfig = $extraConfig;
        return $this;
    }

    /**
     * Check if the config cache is enabled
     *
     * @return bool
     */
    public function getConfigCacheEnabled()
    {
        return $this->configCacheEnabled;
    }

    /**
     * Set if the config cache should be enabled or not
     *
     * @param  bool $enabled
     * @return ListenerOptions
     */
    public function setConfigCacheEnabled($enabled)
    {
        $this->configCacheEnabled = (bool) $enabled;
        return $this;
    }

    /**
     * Get key used to create the cache file name
     *
     * @return string
     */
    public function getConfigCacheKey()
    {
        return (string) $this->configCacheKey;
    }

    /**
     * Set key used to create the cache file name
     *
     * @param  string $configCacheKey the value to be set
     * @return ListenerOptions
     */
    public function setConfigCacheKey($configCacheKey)
    {
        $this->configCacheKey = $configCacheKey;
        return $this;
    }

    /**
     * Get the path to the config cache
     *
     * Should this be an option, or should the dir option include the
     * filename, or should it simply remain hard-coded? Thoughts?
     *
     * @return string
     */
    public function getConfigCacheFile()
    {
        if ($this->getConfigCacheKey()) {
            return $this->getCacheDir() . '/module-config-cache.' . $this->getConfigCacheKey().'.php';
        }

        return $this->getCacheDir() . '/module-config-cache.php';
    }

    /**
     * Get the path where cache file(s) are stored
     *
     * @return string|null
     */
    public function getCacheDir()
    {
        return $this->cacheDir;
    }

    /**
     * Set the path where cache files can be stored
     *
     * @param  string|null $cacheDir the value to be set
     * @return ListenerOptions
     */
    public function setCacheDir($cacheDir)
    {
        $this->cacheDir = $cacheDir ? static::normalizePath($cacheDir) : null;

        return $this;
    }

    /**
     * Check if the module class map cache is enabled
     *
     * @return bool
     */
    public function getModuleMapCacheEnabled()
    {
        return $this->moduleMapCacheEnabled;
    }

    /**
     * Set if the module class map cache should be enabled or not
     *
     * @param  bool $enabled
     * @return ListenerOptions
     */
    public function setModuleMapCacheEnabled($enabled)
    {
        $this->moduleMapCacheEnabled = (bool) $enabled;
        return $this;
    }

    /**
     * Get key used to create the cache file name
     *
     * @return string
     */
    public function getModuleMapCacheKey()
    {
        return (string) $this->moduleMapCacheKey;
    }

    /**
     * Set key used to create the cache file name
     *
     * @param  string $moduleMapCacheKey the value to be set
     * @return ListenerOptions
     */
    public function setModuleMapCacheKey($moduleMapCacheKey)
    {
        $this->moduleMapCacheKey = $moduleMapCacheKey;
        return $this;
    }

    /**
     * Get the path to the module class map cache
     *
     * @return string
     */
    public function getModuleMapCacheFile()
    {
        if ($this->getModuleMapCacheKey()) {
            return $this->getCacheDir() . '/module-classmap-cache.' . $this->getModuleMapCacheKey() . '.php';
        }

        return $this->getCacheDir() . '/module-classmap-cache.php';
    }

    /**
     * Set whether to check dependencies during module loading or not
     *
     * @return bool
     */
    public function getCheckDependencies()
    {
        return $this->checkDependencies;
    }

    /**
     * Set whether to check dependencies during module loading or not
     *
     * @param  bool $checkDependencies the value to be set
     *
     * @return ListenerOptions
     */
    public function setCheckDependencies($checkDependencies)
    {
        $this->checkDependencies = (bool) $checkDependencies;

        return $this;
    }

    /**
     * Whether or not to use zend-loader to autoload modules.
     *
     * @return bool
     */
    public function useZendLoader()
    {
        return $this->useZendLoader;
    }

    /**
     * Set a flag indicating if the module manager should use zend-loader
     *
     * Setting this option to false will disable ModuleAutoloader, requiring
     * other means of autoloading to be used (e.g., Composer).
     *
     * If disabled, the AutoloaderProvider feature will be disabled as well
     *
     * @param  bool $flag
     * @return ListenerOptions
     */
    public function setUseZendLoader($flag)
    {
        $this->useZendLoader = (bool) $flag;
        return $this;
    }

    /**
     * Normalize a path for insertion in the stack
     *
     * @param  string $path
     * @return string
     */
    public static function normalizePath($path)
    {
        $path = rtrim($path, '/');
        $path = rtrim($path, '\\');
        return $path;
    }
}

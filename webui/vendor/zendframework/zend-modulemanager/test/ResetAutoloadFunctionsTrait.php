<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager;

/**
 * Offer common setUp/tearDown methods for preserve current autoload functions and include paths.
 */
trait ResetAutoloadFunctionsTrait
{
    /**
     * @var callable[]
     */
    private $loaders;

    /**
     * @var string
     */
    private $includePath;

    /**
     * @before
     */
    protected function preserveAutoloadFunctions()
    {
        $this->loaders = spl_autoload_functions();
        if (! is_array($this->loaders)) {
            // spl_autoload_functions does not return an empty array when no
            // autoloaders are registered...
            $this->loaders = [];
        }
    }

    /**
     * @before
     */
    protected function preserveIncludePath()
    {
        $this->includePath = get_include_path();
    }

    /**
     * @after
     */
    protected function restoreAutoloadFunctions()
    {
        $loaders = spl_autoload_functions();
        if (is_array($loaders)) {
            foreach ($loaders as $loader) {
                if (! in_array($loader, $this->loaders, true)) {
                    spl_autoload_unregister($loader);
                }
            }
        }
    }

    /**
     * @before
     */
    protected function restoreIncludePath()
    {
        set_include_path((string) $this->includePath);
    }
}

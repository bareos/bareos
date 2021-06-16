<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager;

/**
 * Offer common setUp/tearDown methods for configure a common cache dir.
 */
trait SetUpCacheDirTrait
{
    /**
     * @var string
     */
    protected $tmpdir;

    /**
     * @var string
     */
    protected $configCache;

    /**
     * @before
     */
    protected function createTmpDir()
    {
        $this->tmpdir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'zend_module_cache_dir';
        @mkdir($this->tmpdir);

        $this->configCache = $this->tmpdir . DIRECTORY_SEPARATOR . 'config.cache.php';
    }

    /**
     * @after
     */
    protected function removeTmpDir()
    {
        $file = glob($this->tmpdir . DIRECTORY_SEPARATOR . '*');
        if (isset($file[0])) {
            // change this if there's ever > 1 file
            @unlink($file[0]);
        }
        @rmdir($this->tmpdir);
    }
}

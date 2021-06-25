<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\SimpleCache;

use Cache\IntegrationTests\SimpleCacheTest;
use DirectoryIterator;
use Zend\Cache\Psr\SimpleCache\SimpleCacheDecorator;
use Zend\Cache\Storage\Adapter\Filesystem;
use Zend\Cache\Storage\Adapter\FilesystemOptions;
use Zend\Cache\Storage\Plugin\Serializer;

class FilesystemIntegrationTest extends SimpleCacheTest
{
    /** @var string */
    private $tmpCacheDir;

    /** @var int */
    protected $umask;

    public function setUp()
    {
        $this->umask = umask();

        if (getenv('TESTS_ZEND_CACHE_FILESYSTEM_DIR')) {
            $cacheDir = getenv('TESTS_ZEND_CACHE_FILESYSTEM_DIR');
        } else {
            $cacheDir = sys_get_temp_dir();
        }

        $this->tmpCacheDir = @tempnam($cacheDir, 'zend_cache_test_');
        if (! $this->tmpCacheDir) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory-file: {$err['message']}");
        } elseif (! @unlink($this->tmpCacheDir)) {
            $err = error_get_last();
            $this->fail("Can't remove temporary cache directory-file: {$err['message']}");
        } elseif (! @mkdir($this->tmpCacheDir, 0777)) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory: {$err['message']}");
        }

        $ttlMessage = 'Filesystem adapter does not honor TTL';
        $keyMessage = 'Filesystem adapter supports a subset of PSR-16 characters for keys';

        $this->skippedTests['testSetTtl'] = $ttlMessage;
        $this->skippedTests['testSetMultipleTtl'] = $ttlMessage;
        $this->skippedTests['testSetValidKeys'] = $keyMessage;
        $this->skippedTests['testSetMultipleValidKeys'] = $keyMessage;

        parent::setUp();
    }

    public function tearDown()
    {
        $this->removeRecursive($this->tmpCacheDir);

        if ($this->umask != umask()) {
            umask($this->umask);
            $this->fail('Umask was not reset');
        }

        parent::tearDown();
    }

    public function removeRecursive($dir)
    {
        if (file_exists($dir)) {
            $dirIt = new DirectoryIterator($dir);
            foreach ($dirIt as $entry) {
                $fname = $entry->getFilename();
                if ($fname == '.' || $fname == '..') {
                    continue;
                }

                if ($entry->isFile()) {
                    unlink($entry->getPathname());
                } else {
                    $this->removeRecursive($entry->getPathname());
                }
            }

            rmdir($dir);
        }
    }

    public function createSimpleCache()
    {
        $storage = new Filesystem();
        $storage->setOptions(new FilesystemOptions([
            'cache_dir' => $this->tmpCacheDir,
        ]));
        $storage->addPlugin(new Serializer());

        return new SimpleCacheDecorator($storage);
    }
}

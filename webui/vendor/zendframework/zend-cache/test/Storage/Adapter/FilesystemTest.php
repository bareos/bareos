<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use Zend\Cache;
use Zend\Cache\Storage\Plugin\ExceptionHandler;
use Zend\Cache\Storage\Plugin\PluginOptions;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Filesystem
 */
class FilesystemTest extends CommonAdapterTest
{
    // @codingStandardsIgnoreStart
    protected $_tmpCacheDir;
    protected $_umask;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_umask = umask();

        if (getenv('TESTS_ZEND_CACHE_FILESYSTEM_DIR')) {
            $cacheDir = getenv('TESTS_ZEND_CACHE_FILESYSTEM_DIR');
        } else {
            $cacheDir = sys_get_temp_dir();
        }

        $this->_tmpCacheDir = @tempnam($cacheDir, 'zend_cache_test_');
        if (! $this->_tmpCacheDir) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory-file: {$err['message']}");
        } elseif (! @unlink($this->_tmpCacheDir)) {
            $err = error_get_last();
            $this->fail("Can't remove temporary cache directory-file: {$err['message']}");
        } elseif (! @mkdir($this->_tmpCacheDir, 0777)) {
            $err = error_get_last();
            $this->fail("Can't create temporary cache directory: {$err['message']}");
        }

        $this->_options = new Cache\Storage\Adapter\FilesystemOptions([
            'cache_dir' => $this->_tmpCacheDir,
        ]);
        $this->_storage = new Cache\Storage\Adapter\Filesystem();
        $this->_storage->setOptions($this->_options);

        parent::setUp();
    }

    public function tearDown()
    {
        $this->_removeRecursive($this->_tmpCacheDir);

        if ($this->_umask != umask()) {
            umask($this->_umask);
            $this->fail("Umask wasn't reset");
        }

        parent::tearDown();
    }

    // @codingStandardsIgnoreStart
    protected function _removeRecursive($dir)
    {
        // @codingStandardsIgnoreEnd
        if (file_exists($dir)) {
            $dirIt = new \DirectoryIterator($dir);
            foreach ($dirIt as $entry) {
                $fname = $entry->getFilename();
                if ($fname == '.' || $fname == '..') {
                    continue;
                }

                if ($entry->isFile()) {
                    unlink($entry->getPathname());
                } else {
                    $this->_removeRecursive($entry->getPathname());
                }
            }

            rmdir($dir);
        }
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['filesystem'],
            ['Filesystem'],
        ];
    }

    public function testNormalizeCacheDir()
    {
        $cacheDir = $cacheDirExpected = realpath(sys_get_temp_dir());

        if (DIRECTORY_SEPARATOR != '/') {
            $cacheDir = str_replace(DIRECTORY_SEPARATOR, '/', $cacheDir);
        }

        $firstSlash = strpos($cacheDir, '/');
        $cacheDir = substr($cacheDir, 0, $firstSlash + 1)
                  . '..//../'
                  . substr($cacheDir, $firstSlash)
                  . '///';

        $this->_options->setCacheDir($cacheDir);
        $cacheDir = $this->_options->getCacheDir();

        $this->assertEquals($cacheDirExpected, $cacheDir);
    }

    public function testSetCacheDirToSystemsTempDirWithNull()
    {
        $this->_options->setCacheDir(null);
        $this->assertEquals(sys_get_temp_dir(), $this->_options->getCacheDir());
    }

    public function testSetCacheDirNoDirectoryException()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setCacheDir(__FILE__);
    }

    public function testSetCacheDirNotWritableException()
    {
        if (substr(PHP_OS, 0, 3) == 'WIN') {
            $this->markTestSkipped("Not testable on windows");
        } else {
            @exec('whoami 2>&1', $out, $ret);
            if ($ret) {
                $err = error_get_last();
                $this->markTestSkipped("Not testable: {$err['message']}");
            } elseif (isset($out[0]) && $out[0] == 'root') {
                $this->markTestSkipped("Not testable as root");
            }
        }

        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');

        // create a not writable temporaty directory
        $testDir = tempnam(sys_get_temp_dir(), 'ZendTest');
        unlink($testDir);
        mkdir($testDir);
        chmod($testDir, 0557);

        try {
            $this->_options->setCacheDir($testDir);
        } catch (\Exception $e) {
            rmdir($testDir);
            throw $e;
        }
    }

    public function testSetCacheDirNotReadableException()
    {
        if (substr(PHP_OS, 0, 3) == 'WIN') {
            $this->markTestSkipped("Not testable on windows");
        } else {
            @exec('whoami 2>&1', $out, $ret);
            if ($ret) {
                $this->markTestSkipped("Not testable: " . implode("\n", $out));
            } elseif (isset($out[0]) && $out[0] == 'root') {
                $this->markTestSkipped("Not testable as root");
            }
        }

        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');

        // create a not readable temporaty directory
        $testDir = tempnam(sys_get_temp_dir(), 'ZendTest');
        unlink($testDir);
        mkdir($testDir);
        chmod($testDir, 0337);

        try {
            $this->_options->setCacheDir($testDir);
        } catch (\Exception $e) {
            rmdir($testDir);
            throw $e;
        }
    }

    public function testSetFilePermissionThrowsExceptionIfNotWritable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setFilePermission(0466);
    }

    public function testSetFilePermissionThrowsExceptionIfNotReadable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setFilePermission(0266);
    }

    public function testSetFilePermissionThrowsExceptionIfExecutable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setFilePermission(0661);
    }

    public function testSetNoAtimeChangesAtimeOfMetadataCapability()
    {
        $capabilities = $this->_storage->getCapabilities();

        $this->_options->setNoAtime(false);
        $this->assertContains('atime', $capabilities->getSupportedMetadata());

        $this->_options->setNoAtime(true);
        $this->assertNotContains('atime', $capabilities->getSupportedMetadata());
    }

    public function testSetNoCtimeChangesCtimeOfMetadataCapability()
    {
        $capabilities = $this->_storage->getCapabilities();

        $this->_options->setNoCtime(false);
        $this->assertContains('ctime', $capabilities->getSupportedMetadata());

        $this->_options->setNoCtime(true);
        $this->assertNotContains('ctime', $capabilities->getSupportedMetadata());
    }

    public function testSetDirPermissionThrowsExceptionIfNotWritable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setDirPermission(0577);
    }

    public function testSetDirPermissionThrowsExceptionIfNotReadable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setDirPermission(0377);
    }

    public function testSetDirPermissionThrowsExceptionIfNotExecutable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setDirPermission(0677);
    }

    public function testSetDirLevelInvalidException()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setDirLevel(17); // must between 0-16
    }

    public function testSetUmask()
    {
        $this->_options->setUmask(023);
        $this->assertSame(021, $this->_options->getUmask());

        $this->_options->setUmask(false);
        $this->assertFalse($this->_options->getUmask());
    }

    public function testSetUmaskThrowsExceptionIfNotWritable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setUmask(0300);
    }

    public function testSetUmaskThrowsExceptionIfNotReadable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setUmask(0200);
    }

    public function testSetUmaskThrowsExceptionIfNotExecutable()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setUmask(0100);
    }

    public function testGetMetadataWithCtime()
    {
        $this->_options->setNoCtime(false);

        $this->assertTrue($this->_storage->setItem('test', 'v'));

        $meta = $this->_storage->getMetadata('test');
        $this->assertInternalType('array', $meta);

        $expectedCtime = filectime($meta['filespec'] . '.dat');
        $this->assertEquals($expectedCtime, $meta['ctime']);
    }

    public function testGetMetadataWithAtime()
    {
        $this->_options->setNoAtime(false);

        $this->assertTrue($this->_storage->setItem('test', 'v'));

        $meta = $this->_storage->getMetadata('test');
        $this->assertInternalType('array', $meta);

        $expectedAtime = fileatime($meta['filespec'] . '.dat');
        $this->assertEquals($expectedAtime, $meta['atime']);
    }

    public function testClearExpiredExceptionTriggersEvent()
    {
        $this->_options->setTtl(0.1);
        $this->_storage->setItem('k', 'v');
        $dirs = glob($this->_tmpCacheDir . '/*');
        if (count($dirs) === 0) {
            $this->fail('Could not find cache dir');
        }
        chmod($dirs[0], 0500); //make directory rx, unlink should fail
        sleep(1); //wait for the entry to expire
        $plugin = new ExceptionHandler();
        $options = new PluginOptions(['throw_exceptions' => false]);
        $plugin->setOptions($options);
        $this->_storage->addPlugin($plugin);
        $this->_storage->clearExpired();
        chmod($dirs[0], 0700); //set dir back to writable for tearDown
    }

    public function testClearByNamespaceWithUnexpectedDirectory()
    {
        // create cache items at 2 different directory levels
        $this->_storage->getOptions()->setDirLevel(2);
        $this->_storage->setItem('a_key', 'a_value');
        $this->_storage->getOptions()->setDirLevel(1);
        $this->_storage->setItem('b_key', 'b_value');
        $this->_storage->clearByNamespace($this->_storage->getOptions()->getNamespace());
    }

    public function testClearByPrefixWithUnexpectedDirectory()
    {
        // create cache items at 2 different directory levels
        $this->_storage->getOptions()->setDirLevel(2);
        $this->_storage->setItem('a_key', 'a_value');
        $this->_storage->getOptions()->setDirLevel(1);
        $this->_storage->setItem('b_key', 'b_value');
        $glob = glob($this->_tmpCacheDir.'/*');
        //contrived prefix which will collide with an existing directory
        $prefix = substr(md5('a_key'), 2, 2);
        $this->_storage->clearByPrefix($prefix);
    }

    /**
     * @runInSeparateProcess
     */
    public function testRaceConditionInClearByTags()
    {
        if (! function_exists('pcntl_fork') || ! function_exists('posix_kill')) {
            $this->markTestSkipped('Missing pcntl_fork and/or posix_kill');
        }

        // delay unlink() by global variable $unlinkDelay
        global $unlinkDelay;
        require __DIR__ . '/TestAsset/FilesystemDelayedUnlink.php';

        // create cache items
        $this->_storage->getOptions()->setDirLevel(0);
        $this->_storage->setItems([
            'a_key' => 'a_value',
            'b_key' => 'b_value',
            'other' => 'other',
        ]);
        $this->_storage->setTags('a_key', ['a_tag']);
        $this->_storage->setTags('b_key', ['a_tag']);

        $pidChild = pcntl_fork();
        if ($pidChild == -1) {
            $this->fail('pcntl_fork() failed');
        } elseif ($pidChild) {
            // The parent process
            // Slow down unlink function and start removing items.
            // Finally test if the item not matching the tag was removed by the child process.
            $unlinkDelay = 5000;

            $this->_storage->clearByTags(['a_tag'], true);
            $this->assertFalse($this->_storage->hasItem('other'), 'Child process does not run as expected');
        } else {
            // The child process:
            // Wait to make sure the parent process has started determining files to unlink.
            // Than remove one of the items the parent process should remove and another item for testing.
            usleep(1000);
            $this->_storage->removeItems(['b_key', 'other']);
            posix_kill(posix_getpid(), SIGTERM);
        }
    }

    /**
     * @runInSeparateProcess
     */
    public function testRaceConditionInClearByNamespace()
    {
        if (! function_exists('pcntl_fork') || ! function_exists('posix_kill')) {
            $this->markTestSkipped('Missing pcntl_fork and/or posix_kill');
        }

        // delay unlink() by global variable $unlinkDelay
        global $unlinkDelay;
        require __DIR__ . '/TestAsset/FilesystemDelayedUnlink.php';

        // create cache items
        $this->_storage->getOptions()->setDirLevel(0);
        $this->_storage->getOptions()->setNamespace('ns-other');
        $this->_storage->setItems([
            'other' => 'other',
        ]);
        $this->_storage->getOptions()->setNamespace('ns-4-clear');
        $this->_storage->setItems([
            'a_key' => 'a_value',
            'b_key' => 'b_value',
        ]);

        $pidChild = pcntl_fork();
        if ($pidChild == -1) {
            $this->fail('pcntl_fork() failed');
        } elseif ($pidChild) {
            // The parent process
            // Slow down unlink function and start removing items.
            // Finally test if the item not matching the tag was removed by the child process.
            $unlinkDelay = 5000;

            $this->_storage->getOptions()->setNamespace('ns-4-clear');
            $this->_storage->clearByNamespace('ns-4-clear');

            $this->assertFalse($this->_storage->hasItem('a_key'));
            $this->assertFalse($this->_storage->hasItem('b_key'));

            $this->_storage->getOptions()->setNamespace('ns-other');
            $this->assertFalse($this->_storage->hasItem('other'), 'Child process does not run as expected');
        } else {
            // The child process:
            // Wait to make sure the parent process has started determining files to unlink.
            // Than remove one of the items the parent process should remove and another item for testing.
            usleep(1000);

            $this->_storage->getOptions()->setNamespace('ns-4-clear');
            $this->assertTrue($this->_storage->removeItem('b_key'));

            $this->_storage->getOptions()->setNamespace('ns-other');
            $this->assertTrue($this->_storage->removeItem('other'));

            posix_kill(posix_getpid(), SIGTERM);
        }
    }

    /**
     * @runInSeparateProcess
     */
    public function testRaceConditionInClearByPrefix()
    {
        if (! function_exists('pcntl_fork') || ! function_exists('posix_kill')) {
            $this->markTestSkipped('Missing pcntl_fork and/or posix_kill');
        }

        // delay unlink() by global variable $unlinkDelay
        global $unlinkDelay;
        require __DIR__ . '/TestAsset/FilesystemDelayedUnlink.php';

        // create cache items
        $this->_storage->getOptions()->setDirLevel(0);
        $this->_storage->getOptions()->setNamespace('ns');
        $this->_storage->setItems([
            'prefix_a_key' => 'a_value',
            'prefix_b_key' => 'b_value',
            'other'        => 'other',
        ]);

        $pidChild = pcntl_fork();
        if ($pidChild == -1) {
            $this->fail('pcntl_fork() failed');
        } elseif ($pidChild) {
            // The parent process
            // Slow down unlink function and start removing items.
            // Finally test if the item not matching the tag was removed by the child process.
            $unlinkDelay = 5000;

            $this->_storage->clearByPrefix('prefix_');

            $this->assertFalse($this->_storage->hasItem('prefix_a_key'));
            $this->assertFalse($this->_storage->hasItem('prefix_b_key'));

            $this->assertFalse($this->_storage->hasItem('other'), 'Child process does not run as expected');
        } else {
            // The child process:
            // Wait to make sure the parent process has started determining files to unlink.
            // Than remove one of the items the parent process should remove and another item for testing.
            usleep(1000);

            $this->assertTrue($this->_storage->removeItem('prefix_b_key'));
            $this->assertTrue($this->_storage->removeItem('other'));

            posix_kill(posix_getpid(), SIGTERM);
        }
    }

    /**
     * @runInSeparateProcess
     */
    public function testRaceConditionInClearExpired()
    {
        if (! function_exists('pcntl_fork') || ! function_exists('posix_kill')) {
            $this->markTestSkipped('Missing pcntl_fork and/or posix_kill');
        }

        // delay unlink() by global variable $unlinkDelay
        global $unlinkDelay;
        require __DIR__ . '/TestAsset/FilesystemDelayedUnlink.php';

        // create cache items
        $this->_storage->getOptions()->setDirLevel(0);
        $this->_storage->getOptions()->setTtl(2);
        $this->_storage->setItems([
            'a_key' => 'a_value',
            'b_key' => 'b_value',
            'other' => 'other',
        ]);

        // wait TTL seconds and touch item other so this item will not be deleted by clearExpired
        // and can be used for testing the child process
        $this->waitForFullSecond();
        sleep(2);
        $this->_storage->touchItem('other');

        $pidChild = pcntl_fork();
        if ($pidChild == -1) {
            $this->fail('pcntl_fork() failed');
        } elseif ($pidChild) {
            // The parent process
            // Slow down unlink function and start removing items.
            // Finally test if the item not matching the tag was removed by the child process.
            $unlinkDelay = 5000;

            $this->_storage->clearExpired();

            $this->assertFalse($this->_storage->hasItem('a_key'));
            $this->assertFalse($this->_storage->hasItem('b_key'));

            $this->assertFalse($this->_storage->hasItem('other'), 'Child process does not run as expected');
        } else {
            // The child process:
            // Wait to make sure the parent process has started determining files to unlink.
            // Than remove one of the items the parent process should remove and another item for testing.
            usleep(1000);

            $this->assertTrue($this->_storage->removeItem('b_key'));
            $this->assertTrue($this->_storage->removeItem('other'));

            posix_kill(posix_getpid(), SIGTERM);
        }
    }

    /**
     * @runInSeparateProcess
     */
    public function testRaceConditionInFlush()
    {
        if (! function_exists('pcntl_fork') || ! function_exists('posix_kill')) {
            $this->markTestSkipped('Missing pcntl_fork and/or posix_kill');
        }

        // delay unlink() by global variable $unlinkDelay
        global $unlinkDelay;
        require __DIR__ . '/TestAsset/FilesystemDelayedUnlink.php';

        // create cache items
        $this->_storage->getOptions()->setDirLevel(0);
        $this->_storage->setItems([
            'a_key' => 'a_value',
            'b_key' => 'b_value',
        ]);

        $pidChild = pcntl_fork();
        if ($pidChild == -1) {
            $this->fail('pcntl_fork() failed');
        } elseif ($pidChild) {
            // The parent process
            // Slow down unlink function and start removing items.
            $unlinkDelay = 5000;

            $this->_storage->flush();

            $this->assertFalse($this->_storage->hasItem('a_key'));
            $this->assertFalse($this->_storage->hasItem('b_key'));
        } else {
            // The child process:
            // Wait to make sure the parent process has started determining files to unlink.
            // Than remove one of the items the parent process should remove.
            usleep(1000);

            $this->assertTrue($this->_storage->removeItem('b_key'));

            posix_kill(posix_getpid(), SIGTERM);
        }
    }

    public function testSuffixIsMutable()
    {
        $this->_options->setSuffix('.cache');
        $this->assertSame('.cache', $this->_options->getSuffix());
    }

    public function testTagSuffixIsMutable()
    {
        $this->_options->setTagSuffix('.cache');
        $this->assertSame('.cache', $this->_options->getTagSuffix());
    }

    public function testEmptyTagsArrayClearsTags()
    {
        $key = 'key';
        $tags = ['tag1', 'tag2', 'tag3'];
        $this->assertTrue($this->_storage->setItem($key, 100));
        $this->assertTrue($this->_storage->setTags($key, $tags));
        $this->assertNotEmpty($this->_storage->getTags($key));
        $this->assertTrue($this->_storage->setTags($key, []));
        $this->assertEmpty($this->_storage->getTags($key));
    }
}

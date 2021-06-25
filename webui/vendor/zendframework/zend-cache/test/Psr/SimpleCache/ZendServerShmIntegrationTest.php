<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\SimpleCache;

use Cache\IntegrationTests\SimpleCacheTest;
use Zend\Cache\Psr\SimpleCache\SimpleCacheDecorator;
use Zend\Cache\StorageFactory;
use Zend\Cache\Exception;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;

class ZendServerShmIntegrationTest extends SimpleCacheTest
{
    /**
     * Backup default timezone
     * @var string
     */
    private $tz;

    protected function setUp()
    {
        if (! getenv('TESTS_ZEND_CACHE_ZEND_SERVER_ENABLED')) {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_ZEND_SERVER_ENABLED to run this test');
        }

        if (! function_exists('zend_shm_cache_store') || PHP_SAPI == 'cli') {
            $this->markTestSkipped("Missing 'zend_shm_cache_*' functions or running from SAPI 'cli'");
        }

        // set non-UTC timezone
        $this->tz = date_default_timezone_get();
        date_default_timezone_set('America/Vancouver');

        parent::setUp();
    }

    protected function tearDown()
    {
        date_default_timezone_set($this->tz);

        if (function_exists('zend_shm_cache_clear')) {
            zend_disk_cache_clear();
        }

        parent::tearDown();
    }

    public function createSimpleCache()
    {
        try {
            $storage = StorageFactory::adapterFactory('zendservershm');
            return new SimpleCacheDecorator($storage);
        } catch (Exception\ExtensionNotLoadedException $e) {
            $this->markTestSkipped($e->getMessage());
        } catch (ServiceNotCreatedException $e) {
            if ($e->getPrevious() instanceof Exception\ExtensionNotLoadedException) {
                $this->markTestSkipped($e->getMessage());
            }
            throw $e;
        }
    }
}

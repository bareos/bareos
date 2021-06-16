<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\CacheItemPool;

use Cache\IntegrationTests\CachePoolTest;
use MongoDB\Client;
use Zend\Cache\Psr\CacheItemPool\CacheItemPoolDecorator;
use Zend\Cache\Storage\Adapter\ExtMongoDb;
use Zend\Cache\StorageFactory;
use Zend\Cache\Exception;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;

class ExtMongoDbIntegrationTest extends CachePoolTest
{
    /**
     * Backup default timezone
     * @var string
     */
    private $tz;

    /**
     * @var ExtMongoDb
     */
    private $storage;

    protected function setUp()
    {
        if (! getenv('TESTS_ZEND_CACHE_EXTMONGODB_ENABLED')) {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_EXTMONGODB_ENABLED to run this test');
        }

        if (! extension_loaded('mongodb') || ! class_exists(Client::class)) {
            $this->markTestSkipped("mongodb extension is not loaded");
        }

        // set non-UTC timezone
        $this->tz = date_default_timezone_get();
        date_default_timezone_set('America/Vancouver');

        parent::setUp();
    }

    protected function tearDown()
    {
        date_default_timezone_set($this->tz);

        if ($this->storage) {
            $this->storage->flush();
        }

        parent::tearDown();
    }

    public function createCachePool()
    {
        $storage = StorageFactory::adapterFactory('extmongodb', [
            'server'     => getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'),
            'database'   => getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            'collection' => getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION'),
        ]);

        $deferredSkippedMessage = sprintf(
            '%s storage doesn\'t support driver deferred',
            \get_class($storage)
        );
        $this->skippedTests['testHasItemReturnsFalseWhenDeferredItemIsExpired'] = $deferredSkippedMessage;

        return new CacheItemPoolDecorator($storage);
    }
}

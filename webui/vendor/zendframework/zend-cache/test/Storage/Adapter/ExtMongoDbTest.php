<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use MongoDB\Client;
use Zend\Cache\Storage\Adapter\ExtMongoDb;
use Zend\Cache\Storage\Adapter\ExtMongoDbOptions;

/**
 * @covers Zend\Cache\Storage\Adapter\ExtMongoDb<extended>
 */
class ExtMongoDbTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_EXTMONGODB_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_MONGODB_ENABLED to run this test');
        }

        if (! extension_loaded('mongodb') || ! class_exists(Client::class)) {
            $this->markTestSkipped("mongodb extension is not loaded");
        }

        $this->_options = new ExtMongoDbOptions([
            'server'     => getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'),
            'database'   => getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            'collection' => getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION'),
        ]);

        $this->_storage = new ExtMongoDb();
        $this->_storage->setOptions($this->_options);
        $this->_storage->flush();

        parent::setUp();
    }

    public function tearDown()
    {
        if ($this->_storage) {
            $this->_storage->flush();
        }

        parent::tearDown();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['ext_mongo_db'],
            ['extmongodb'],
            ['extMongoDb'],
            ['extMongoDB'],
            ['ExtMongoDb'],
            ['ExtMongoDB'],
        ];
    }

    public function testSetOptionsNotMongoDbOptions()
    {
        $this->_storage->setOptions([
            'server'     => getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'),
            'database'   => getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            'collection' => getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION'),
        ]);

        $this->assertInstanceOf(ExtMongoDbOptions::class, $this->_storage->getOptions());
    }
}

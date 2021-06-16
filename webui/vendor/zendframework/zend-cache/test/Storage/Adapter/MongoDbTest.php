<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use Zend\Cache\Storage\Adapter\MongoDb;
use Zend\Cache\Storage\Adapter\MongoDbOptions;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\MongoDb<extended>
 */
class MongoDbTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_MONGODB_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_MONGODB_ENABLED to run this test');
        }

        if (! extension_loaded('mongo') || ! class_exists('\Mongo') || ! class_exists('\MongoClient')) {
            // Allow tests to run if Mongo extension is loaded, or we have a polyfill in place
            $this->markTestSkipped("Mongo extension is not loaded");
        }

        $this->_options = new MongoDbOptions([
            'server'     => getenv('TESTS_ZEND_CACHE_MONGODB_CONNECTSTRING'),
            'database'   => getenv('TESTS_ZEND_CACHE_MONGODB_DATABASE'),
            'collection' => getenv('TESTS_ZEND_CACHE_MONGODB_COLLECTION'),
        ]);

        $this->_storage = new MongoDb();
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
            ['mongo_db'],
            ['mongodb'],
            ['MongoDb'],
            ['MongoDB'],
        ];
    }

    public function testSetOptionsNotMongoDbOptions()
    {
        $this->_storage->setOptions([
            'server'     => getenv('TESTS_ZEND_CACHE_MONGODB_CONNECTSTRING'),
            'database'   => getenv('TESTS_ZEND_CACHE_MONGODB_DATABASE'),
            'collection' => getenv('TESTS_ZEND_CACHE_MONGODB_COLLECTION'),
        ]);

        $this->assertInstanceOf('\Zend\Cache\Storage\Adapter\MongoDbOptions', $this->_storage->getOptions());
    }
}

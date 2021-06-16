<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use MongoDB\Client;
use MongoDB\Collection;
use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\Cache\Exception;
use Zend\Cache\Storage\Adapter\ExtMongoDbResourceManager;

/**
 * @covers Zend\Cache\Storage\Adapter\ExtMongoDbResourceManager
 */
class ExtMongoDbResourceManagerTest extends TestCase
{
    /**
     * @var ExtMongoDbResourceManager
     */
    protected $object;

    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_EXTMONGODB_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_EXTMONGODB_ENABLED to run this test');
        }

        if (! extension_loaded('mongodb') || ! class_exists(Client::class)) {
            $this->markTestSkipped("mongodb extension is not loaded");
        }

        $this->object = new ExtMongoDbResourceManager();
    }

    public function testSetResourceAlreadyCreated()
    {
        $this->assertAttributeEmpty('resources', $this->object);

        $id = 'foo';

        $client = new Client(getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'));
        $resource = $client->selectCollection(
            getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION')
        );

        $this->object->setResource($id, $resource);

        $this->assertSame($resource, $this->object->getResource($id));
    }

    public function testSetResourceArray()
    {
        $this->assertAttributeEmpty('resources', $this->object);

        $id     = 'foo';
        $server = 'mongodb://test:1234';

        $this->object->setResource($id, ['server' => $server]);

        $this->assertSame($server, $this->object->getServer($id));
    }

    public function testSetResourceThrowsException()
    {
        $id = 'foo';
        $resource = new stdClass();

        $this->expectException(Exception\InvalidArgumentException::class);
        $this->object->setResource($id, $resource);
    }

    public function testHasResourceEmpty()
    {
        $id = 'foo';

        $this->assertFalse($this->object->hasResource($id));
    }

    public function testHasResourceSet()
    {
        $id = 'foo';

        $this->object->setResource($id, ['foo' => 'bar']);

        $this->assertTrue($this->object->hasResource($id));
    }

    public function testGetResourceNotSet()
    {
        $id = 'foo';

        $this->assertFalse($this->object->hasResource($id));

        $this->expectException(Exception\RuntimeException::class);
        $this->object->getResource($id);
    }

    public function testGetResourceInitialized()
    {
        $id = 'foo';

        $client = new Client(getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'));
        $resource = $client->selectCollection(
            getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION')
        );

        $this->object->setResource($id, $resource);

        $this->assertSame($resource, $this->object->getResource($id));
    }

    public function testCorrectDatabaseResourceName()
    {
        $id = 'foo';

        $resource = [
            'db' => getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE'),
            'server' => getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING'),
        ];

        $this->object->setResource($id, $resource);

        $this->assertSame($resource['db'], $this->object->getResource($id)->getDatabaseName());
    }

    public function testGetResourceNewResource()
    {
        $id                = 'foo';
        $server            = getenv('TESTS_ZEND_CACHE_EXTMONGODB_CONNECTSTRING');
        $connectionOptions = ['connectTimeoutMS' => 5];
        $database          = getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE');
        $collection        = getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION');

        $this->object->setServer($id, $server);
        $this->object->setConnectionOptions($id, $connectionOptions);
        $this->object->setDatabase($id, $database);
        $this->object->setCollection($id, $collection);

        $this->assertInstanceOf(Collection::class, $this->object->getResource($id));
    }

    public function testGetResourceUnknownServerThrowsException()
    {
        $id                = 'foo';
        $server            = 'mongodb://unknown.unknown';
        $connectionOptions = ['connectTimeoutMS' => 5];
        $database          = getenv('TESTS_ZEND_CACHE_EXTMONGODB_DATABASE');
        $collection        = getenv('TESTS_ZEND_CACHE_EXTMONGODB_COLLECTION');

        $this->object->setServer($id, $server);
        $this->object->setConnectionOptions($id, $connectionOptions);
        $this->object->setDatabase($id, $database);
        $this->object->setCollection($id, $collection);

        $this->expectException(Exception\RuntimeException::class);
        $this->object->getResource($id);
    }

    public function testGetSetCollection()
    {
        $resourceId     = 'testResource';
        $collectionName = 'testCollection';

        $this->object->setCollection($resourceId, $collectionName);
        $this->assertSame($collectionName, $this->object->getCollection($resourceId));
    }

    public function testGetSetConnectionOptions()
    {
        $resourceId        = 'testResource';
        $connectionOptions = ['test1' => 'option1', 'test2' => 'option2'];

        $this->object->setConnectionOptions($resourceId, $connectionOptions);
        $this->assertSame($connectionOptions, $this->object->getConnectionOptions($resourceId));
    }

    public function testGetSetServer()
    {
        $resourceId = 'testResource';
        $server     = 'testServer';

        $this->object->setServer($resourceId, $server);
        $this->assertSame($server, $this->object->getServer($resourceId));
    }

    public function testGetSetDriverOptions()
    {
        $resourceId    = 'testResource';
        $driverOptions = ['test1' => 'option1', 'test2' => 'option2'];

        $this->object->setDriverOptions($resourceId, $driverOptions);
        $this->assertSame($driverOptions, $this->object->getDriverOptions($resourceId));
    }

    public function testGetSetDatabase()
    {
        $resourceId = 'testResource';
        $database   = 'testDatabase';

        $this->object->setDatabase($resourceId, $database);
        $this->assertSame($database, $this->object->getDatabase($resourceId));
    }
}

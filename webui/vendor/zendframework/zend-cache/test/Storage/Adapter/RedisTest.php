<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit_Framework_MockObject_MockObject;
use Zend\Cache;
use Redis as RedisResource;
use Zend\Cache\Storage\Adapter\Redis;
use Zend\Cache\Storage\Adapter\RedisOptions;
use Zend\Cache\Storage\Adapter\RedisResourceManager;

/**
 * @covers Zend\Cache\Storage\Adapter\Redis<extended>
 */
class RedisTest extends CommonAdapterTest
{
    // @codingStandardsIgnoreStart
    /**
     *
     * @var Cache\Storage\Adapter\RedisOptions
     */
    protected $_options;

    /**
     *
     * @var Cache\Storage\Adapter\Redis
     */
    protected $_storage;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_REDIS_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_REDIS_ENABLED to run this test');
        }

        if (! extension_loaded('redis')) {
            $this->markTestSkipped("Redis extension is not loaded");
        }

        $options = ['resource_id' => __CLASS__];

        if (getenv('TESTS_ZEND_CACHE_REDIS_HOST') && getenv('TESTS_ZEND_CACHE_REDIS_PORT')) {
            $options['server'] = [getenv('TESTS_ZEND_CACHE_REDIS_HOST'), getenv('TESTS_ZEND_CACHE_REDIS_PORT')];
        } elseif (getenv('TESTS_ZEND_CACHE_REDIS_HOST')) {
            $options['server'] = [getenv('TESTS_ZEND_CACHE_REDIS_HOST')];
        }

        if (getenv('TESTS_ZEND_CACHE_REDIS_DATABASE')) {
            $options['database'] = getenv('TESTS_ZEND_CACHE_REDIS_DATABASE');
        }

        if (getenv('TESTS_ZEND_CACHE_REDIS_PASSWORD')) {
            $options['password'] = getenv('TESTS_ZEND_CACHE_REDIS_PASSWORD');
        }

        $this->_options = new Cache\Storage\Adapter\RedisOptions($options);
        $this->_storage = new Cache\Storage\Adapter\Redis($this->_options);

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
            ['redis'],
            ['Redis'],
        ];
    }

    public function testLibOptionsFirst()
    {
        $options = [
            'resource_id' => __CLASS__ . '2',
            'lib_options' => [
                RedisResource::OPT_SERIALIZER => RedisResource::SERIALIZER_PHP,
            ],
        ];

        if (getenv('TESTS_ZEND_CACHE_REDIS_HOST') && getenv('TESTS_ZEND_CACHE_REDIS_PORT')) {
            $options['server'] = [getenv('TESTS_ZEND_CACHE_REDIS_HOST'), getenv('TESTS_ZEND_CACHE_REDIS_PORT')];
        } elseif (getenv('TESTS_ZEND_CACHE_REDIS_HOST')) {
            $options['server'] = [getenv('TESTS_ZEND_CACHE_REDIS_HOST')];
        }

        if (getenv('TESTS_ZEND_CACHE_REDIS_DATABASE')) {
            $options['database'] = getenv('TESTS_ZEND_CACHE_REDIS_DATABASE');
        }

        if (getenv('TESTS_ZEND_CACHE_REDIS_PASSWORD')) {
            $options['password'] = getenv('TESTS_ZEND_CACHE_REDIS_PASSWORD');
        }

        $redisOptions = new Cache\Storage\Adapter\RedisOptions($options);
        $storage = new Cache\Storage\Adapter\Redis($redisOptions);

        $this->assertInstanceOf('Zend\\Cache\\Storage\\Adapter\\Redis', $storage);
    }

    public function testRedisSerializer()
    {
        $this->_storage->addPlugin(new \Zend\Cache\Storage\Plugin\Serializer());
        $value = ['test', 'of', 'array'];
        $this->_storage->setItem('key', $value);

        $this->assertCount(count($value), $this->_storage->getItem('key'), 'Problem with Redis serialization');
    }

    public function testRedisSetInt()
    {
        $key = 'key';
        $this->assertTrue($this->_storage->setItem($key, 123));
        $this->assertEquals('123', $this->_storage->getItem($key), 'Integer should be cast to string');
    }

    public function testRedisSetDouble()
    {
        $key = 'key';
        $this->assertTrue($this->_storage->setItem($key, 123.12));
        $this->assertEquals('123.12', $this->_storage->getItem($key), 'Integer should be cast to string');
    }

    public function testRedisSetNull()
    {
        $key = 'key';
        $this->assertTrue($this->_storage->setItem($key, null));
        $this->assertEquals('', $this->_storage->getItem($key), 'Null should be cast to string');
    }

    public function testRedisSetBoolean()
    {
        $key = 'key';
        $this->assertTrue($this->_storage->setItem($key, true));
        $this->assertEquals('1', $this->_storage->getItem($key), 'Boolean should be cast to string');
        $this->assertTrue($this->_storage->setItem($key, false));
        $this->assertEquals('', $this->_storage->getItem($key), 'Boolean should be cast to string');
    }

    public function testGetCapabilitiesTtl()
    {
        $resourceManager = $this->_options->getResourceManager();
        $resourceId      = $this->_options->getResourceId();
        $redis = $resourceManager->getResource($resourceId);
        $majorVersion = (int) $redis->info()['redis_version'];

        $this->assertEquals($majorVersion, $resourceManager->getMajorVersion($resourceId));

        $capabilities = $this->_storage->getCapabilities();
        if ($majorVersion < 2) {
            $this->assertEquals(0, $capabilities->getMinTtl(), 'Redis version < 2.0.0 does not support key expiration');
        } else {
            $this->assertEquals(1, $capabilities->getMinTtl(), 'Redis version > 2.0.0 supports key expiration');
        }
    }

    /* ResourceManager */

    public function testSocketConnection()
    {
        $socket = '/tmp/redis.sock';
        $this->_options->getResourceManager()->setServer($this->_options->getResourceId(), $socket);
        $normalized = $this->_options->getResourceManager()->getServer($this->_options->getResourceId());
        $this->assertEquals($socket, $normalized['host'], 'Host should equal to socket {$socket}');

        // Don't try to flush on shutdown
        $this->_storage = null;
    }

    public function testGetSetDatabase()
    {
        $this->assertTrue($this->_storage->setItem('key', 'val'));
        $this->assertEquals('val', $this->_storage->getItem('key'));

        $databaseNumber = 1;
        $resourceManager = $this->_options->getResourceManager();
        $resourceManager->setDatabase($this->_options->getResourceId(), $databaseNumber);
        $this->assertNull(
            $this->_storage->getItem('key'),
            'No value should be found because set was done on different database than get'
        );
        $this->assertEquals(
            $databaseNumber,
            $resourceManager->getDatabase($this->_options->getResourceId()),
            'Incorrect database was returned'
        );
    }

    public function testGetSetPassword()
    {
        $pass = 'super secret';
        $this->_options->getResourceManager()->setPassword($this->_options->getResourceId(), $pass);
        $this->assertEquals(
            $pass,
            $this->_options->getResourceManager()->getPassword($this->_options->getResourceId()),
            'Password was not correctly set'
        );
    }

    public function testGetSetLibOptionsOnExistingRedisResourceInstance()
    {
        $options = ['serializer' => RedisResource::SERIALIZER_PHP];
        $this->_options->setLibOptions($options);

        $value  = ['value'];
        $key    = 'key';
        //test if it's still possible to set/get item and if lib serializer works
        $this->_storage->setItem($key, $value);

        $this->assertEquals(
            $value,
            $this->_storage->getItem($key),
            'Redis should return an array, lib options were not set correctly'
        );

        $options = ['serializer' => RedisResource::SERIALIZER_NONE];
        $this->_options->setLibOptions($options);
        $this->_storage->setItem($key, $value);
        //should not serialize array correctly
        $this->assertNotInternalType(
            'array',
            $this->_storage->getItem($key),
            'Redis should not serialize automatically anymore, lib options were not set correctly'
        );
    }

    public function testGetSetLibOptionsWithCleanRedisResourceInstance()
    {
        $options = ['serializer' => RedisResource::SERIALIZER_PHP];
        $this->_options->setLibOptions($options);

        $redis = new Cache\Storage\Adapter\Redis($this->_options);
        $value  = ['value'];
        $key    = 'key';
        //test if it's still possible to set/get item and if lib serializer works
        $redis->setItem($key, $value);
        $this->assertEquals(
            $value,
            $redis->getItem($key),
            'Redis should return an array, lib options were not set correctly'
        );


        $options = ['serializer' => RedisResource::SERIALIZER_NONE];
        $this->_options->setLibOptions($options);
        $redis->setItem($key, $value);
        //should not serialize array correctly
        $this->assertNotInternalType(
            'array',
            $redis->getItem($key),
            'Redis should not serialize automatically anymore, lib options were not set correctly'
        );
    }

    /* RedisOptions */

    public function testGetSetNamespace()
    {
        $namespace = 'testNamespace';
        $this->_options->setNamespace($namespace);
        $this->assertEquals($namespace, $this->_options->getNamespace(), 'Namespace was not set correctly');
    }

    public function testGetSetNamespaceSeparator()
    {
        $separator = '/';
        $this->_options->setNamespaceSeparator($separator);
        $this->assertEquals($separator, $this->_options->getNamespaceSeparator(), 'Separator was not set correctly');
    }

    public function testGetSetResourceManager()
    {
        $resourceManager = new \Zend\Cache\Storage\Adapter\RedisResourceManager();
        $options = new \Zend\Cache\Storage\Adapter\RedisOptions();
        $options->setResourceManager($resourceManager);
        $this->assertInstanceOf(
            'Zend\\Cache\\Storage\\Adapter\\RedisResourceManager',
            $options->getResourceManager(),
            'Wrong resource manager retuned, it should of type RedisResourceManager'
        );

        $this->assertEquals($resourceManager, $options->getResourceManager());
    }

    public function testGetSetResourceId()
    {
        $resourceId = '1';
        $options = new \Zend\Cache\Storage\Adapter\RedisOptions();
        $options->setResourceId($resourceId);
        $this->assertEquals($resourceId, $options->getResourceId(), 'Resource id was not set correctly');
    }

    public function testGetSetPersistentId()
    {
        $persistentId = '1';
        $this->_options->setPersistentId($persistentId);
        $this->assertEquals($persistentId, $this->_options->getPersistentId(), 'Persistent id was not set correctly');
    }

    public function testOptionsGetSetLibOptions()
    {
        $options = ['serializer' => RedisResource::SERIALIZER_PHP];
        $this->_options->setLibOptions($options);
        $this->assertEquals(
            $options,
            $this->_options->getLibOptions(),
            'Lib Options were not set correctly through RedisOptions'
        );
    }

    public function testGetSetServer()
    {
        $server = [
            'host' => '127.0.0.1',
            'port' => 6379,
            'timeout' => 0,
        ];
        $this->_options->setServer($server);
        $this->assertEquals($server, $this->_options->getServer(), 'Server was not set correctly through RedisOptions');

        // Don't try to flush on shutdown
        $this->_storage = null;
    }

    public function testOptionsGetSetDatabase()
    {
        $database = 1;
        $this->_options->setDatabase($database);
        $this->assertEquals($database, $this->_options->getDatabase(), 'Database not set correctly using RedisOptions');
    }

    public function testOptionsGetSetPassword()
    {
        $password = 'my-secret';
        $this->_options->setPassword($password);
        $this->assertEquals(
            $password,
            $this->_options->getPassword(),
            'Password was set incorrectly using RedisOptions'
        );
    }

    public function testTouchItem()
    {
        $key = 'key';

        // no TTL
        $this->_storage->getOptions()->setTtl(0);
        $this->_storage->setItem($key, 'val');
        $this->assertEquals(0, $this->_storage->getMetadata($key)['ttl']);

        // touch with a specific TTL will add this TTL
        $ttl = 1000;
        $this->_storage->getOptions()->setTtl($ttl);
        $this->assertTrue($this->_storage->touchItem($key));
        $this->assertEquals($ttl, ceil($this->_storage->getMetadata($key)['ttl']));
    }

    public function testHasItemReturnsFalseIfRedisExistsReturnsZero()
    {
        $redis = $this->mockInitializedRedisResource();
        $redis->method('exists')->willReturn(0);
        $adapter = $this->createAdapterFromResource($redis);

        $hasItem = $adapter->hasItem('does-not-exist');

        $this->assertFalse($hasItem);
    }

    public function testHasItemReturnsTrueIfRedisExistsReturnsNonZeroInt()
    {
        $redis = $this->mockInitializedRedisResource();
        $redis->method('exists')->willReturn(23);
        $adapter = $this->createAdapterFromResource($redis);

        $hasItem = $adapter->hasItem('does-not-exist');

        $this->assertTrue($hasItem);
    }

    /**
     * @return Redis
     */
    private function createAdapterFromResource(RedisResource $redis)
    {
        $resourceManager = new RedisResourceManager();
        $resourceId = 'my-resource';
        $resourceManager->setResource($resourceId, $redis);
        $options = new RedisOptions(['resource_manager' => $resourceManager, 'resource_id' => $resourceId]);
        return new Redis($options);
    }

    /**
     * @return PHPUnit_Framework_MockObject_MockObject|RedisResource
     */
    private function mockInitializedRedisResource()
    {
        $redis = $this->getMockBuilder(RedisResource::class)->getMock();
        $redis->socket = true;
        return $redis;
    }
}

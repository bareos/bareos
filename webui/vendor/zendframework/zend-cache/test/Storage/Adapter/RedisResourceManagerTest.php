<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\Adapter\RedisResourceManager;

/**
 * PHPUnit test case
 */

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\RedisResourceManager
 */
class RedisResourceManagerTest extends TestCase
{
    /**
     * The resource manager
     *
     * @var RedisResourceManager
     */
    protected $resourceManager;

    public function setUp()
    {
        $this->resourceManager = new RedisResourceManager();
    }

    /**
     * @group 6495
     */
    public function testSetServerWithPasswordInUri()
    {
        $dummyResId = '1234567890';
        $server     = 'redis://dummyuser:dummypass@testhost:1234';

        $this->resourceManager->setServer($dummyResId, $server);

        $server = $this->resourceManager->getServer($dummyResId);

        $this->assertEquals('testhost', $server['host']);
        $this->assertEquals(1234, $server['port']);
        $this->assertEquals('dummypass', $this->resourceManager->getPassword($dummyResId));
    }

    /**
     * @group 6495
     */
    public function testSetServerWithPasswordInParameters()
    {
        $server      = 'redis://dummyuser:dummypass@testhost:1234';
        $dummyResId2 = '12345678901';
        $resource    = [
            'persistent_id' => 'my_connection_name',
            'server'        => $server,
            'password'      => 'abcd1234'
        ];

        $this->resourceManager->setResource($dummyResId2, $resource);

        $server = $this->resourceManager->getServer($dummyResId2);

        $this->assertEquals('testhost', $server['host']);
        $this->assertEquals(1234, $server['port']);
        $this->assertEquals('abcd1234', $this->resourceManager->getPassword($dummyResId2));
    }

    /**
     * @group 6495
     */
    public function testSetServerWithPasswordInUriShouldNotOverridePreviousResource()
    {
        $server      = 'redis://dummyuser:dummypass@testhost:1234';
        $server2     = 'redis://dummyuser:dummypass@testhost2:1234';
        $dummyResId2 = '12345678901';
        $resource    = [
            'persistent_id' => 'my_connection_name',
            'server'        => $server,
            'password'      => 'abcd1234'
        ];

        $this->resourceManager->setResource($dummyResId2, $resource);
        $this->resourceManager->setServer($dummyResId2, $server2);

        $server = $this->resourceManager->getServer($dummyResId2);

        $this->assertEquals('testhost2', $server['host']);
        $this->assertEquals(1234, $server['port']);
        // Password should not be overridden
        $this->assertEquals('abcd1234', $this->resourceManager->getPassword($dummyResId2));
    }

    /**
     * Test with 'persistent_id'
     */
    public function testValidPersistentId()
    {
        if (getenv('TESTS_ZEND_CACHE_REDIS_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_REDIS_ENABLED to run this test');
        }

        if (! extension_loaded('redis')) {
            $this->markTestSkipped("Redis extension is not loaded");
        }

        $resourceId = 'testValidPersistentId';
        $resource   = [
            'persistent_id' => 'my_connection_name',
            'server' => [
                'host' => getenv('TESTS_ZEND_CACHE_REDIS_HOST') ?: 'localhost',
                'port' => getenv('TESTS_ZEND_CACHE_REDIS_PORT') ?: 6379,
            ],
        ];
        $expectedPersistentId = 'my_connection_name';
        $this->resourceManager->setResource($resourceId, $resource);
        $this->assertSame($expectedPersistentId, $this->resourceManager->getPersistentId($resourceId));
        $this->assertInstanceOf('Redis', $this->resourceManager->getResource($resourceId));
    }

    /**
     * Test with 'persistend_id' instead of 'persistent_id'
     */
    public function testNotValidPersistentIdOptionName()
    {
        if (getenv('TESTS_ZEND_CACHE_REDIS_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_REDIS_ENABLED to run this test');
        }

        if (! extension_loaded('redis')) {
            $this->markTestSkipped("Redis extension is not loaded");
        }

        $resourceId = 'testNotValidPersistentId';
        $resource   = [
            'persistend_id' => 'my_connection_name',
            'server' => [
                'host' => getenv('TESTS_ZEND_CACHE_REDIS_HOST') ?: 'localhost',
                'port' => getenv('TESTS_ZEND_CACHE_REDIS_PORT') ?: 6379,
            ],
        ];
        $expectedPersistentId = 'my_connection_name';
        $this->resourceManager->setResource($resourceId, $resource);

        $this->assertNotSame($expectedPersistentId, $this->resourceManager->getPersistentId($resourceId));
        $this->assertEmpty($this->resourceManager->getPersistentId($resourceId));
        $this->assertInstanceOf('Redis', $this->resourceManager->getResource($resourceId));
    }

    public function testGetVersion()
    {
        if (getenv('TESTS_ZEND_CACHE_REDIS_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_REDIS_ENABLED to run this test');
        }

        if (! extension_loaded('redis')) {
            $this->markTestSkipped("Redis extension is not loaded");
        }

        $resourceId = __FUNCTION__;
        $resource   = [
            'server' => [
                'host' => getenv('TESTS_ZEND_CACHE_REDIS_HOST') ?: 'localhost',
                'port' => getenv('TESTS_ZEND_CACHE_REDIS_PORT') ?: 6379,
            ],
        ];
        $this->resourceManager->setResource($resourceId, $resource);

        $this->assertRegExp('/^\d+\.\d+\.\d+/', $this->resourceManager->getVersion($resourceId));
    }

    public function testGetMajorVersion()
    {
        if (getenv('TESTS_ZEND_CACHE_REDIS_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_REDIS_ENABLED to run this test');
        }

        if (! extension_loaded('redis')) {
            $this->markTestSkipped("Redis extension is not loaded");
        }

        $resourceId = __FUNCTION__;
        $resource   = [
            'server' => [
                'host' => getenv('TESTS_ZEND_CACHE_REDIS_HOST') ?: 'localhost',
                'port' => getenv('TESTS_ZEND_CACHE_REDIS_PORT') ?: 6379,
            ],
        ];
        $this->resourceManager->setResource($resourceId, $resource);

        $this->assertGreaterThan(0, $this->resourceManager->getMajorVersion($resourceId));
    }
}

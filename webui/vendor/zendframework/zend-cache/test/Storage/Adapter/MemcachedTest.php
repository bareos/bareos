<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use Prophecy\Argument;
use Zend\Cache;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Memcached<extended>
 */
class MemcachedTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_MEMCACHED_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_MEMCACHED_ENABLED to run this test');
        }

        if (! extension_loaded('memcached')) {
            $this->markTestSkipped("Memcached extension is not loaded");
        }

        $this->_options  = new Cache\Storage\Adapter\MemcachedOptions([
            'resource_id' => __CLASS__
        ]);

        if (getenv('TESTS_ZEND_CACHE_MEMCACHED_HOST') && getenv('TESTS_ZEND_CACHE_MEMCACHED_PORT')) {
            $this->_options->getResourceManager()->setServers(__CLASS__, [
                [getenv('TESTS_ZEND_CACHE_MEMCACHED_HOST'), getenv('TESTS_ZEND_CACHE_MEMCACHED_PORT')]
            ]);
        } elseif (getenv('TESTS_ZEND_CACHE_MEMCACHED_HOST')) {
            $this->_options->getResourceManager()->setServers(__CLASS__, [
                [getenv('TESTS_ZEND_CACHE_MEMCACHED_HOST')]
            ]);
        }

        $this->_storage = new Cache\Storage\Adapter\Memcached();
        $this->_storage->setOptions($this->_options);
        $this->_storage->flush();

        parent::setUp();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['memcached'],
            ['Memcached'],
        ];
    }

    /**
     * @deprecated
     */
    public function testOptionsAddServer()
    {
        $options = new Cache\Storage\Adapter\MemcachedOptions();

        $deprecated = false;
        set_error_handler(function () use (& $deprecated) {
            $deprecated = true;
        }, E_USER_DEPRECATED);

        $options->addServer('127.0.0.1', 11211);
        $options->addServer('localhost');
        $options->addServer('domain.com', 11215);

        restore_error_handler();
        $this->assertTrue($deprecated, 'Missing deprecated error');

        $servers = [
            ['host' => '127.0.0.1', 'port' => 11211, 'weight' => 0],
            ['host' => 'localhost', 'port' => 11211, 'weight' => 0],
            ['host' => 'domain.com', 'port' => 11215, 'weight' => 0],
        ];

        $this->assertEquals($options->getServers(), $servers);
        $memcached = new Cache\Storage\Adapter\Memcached($options);
        $this->assertEquals($memcached->getOptions()->getServers(), $servers);
    }

    public function testMemcachedReturnsSuccessFalseOnError()
    {
        if (! defined('Memcached::GET_EXTENDED')) {
            $this->markTestSkipped('Test skipped because Memcached < 3.0 with Memcached::GET_EXTENDED not defined');
            return;
        }

        $resource = $this->prophesize(\Memcached::class);
        $resourceManager = $this->prophesize(Cache\Storage\Adapter\MemcachedResourceManager::class);

        $resourceManager->getResource(Argument::any())->willReturn($resource->reveal());
        $resource->get(Argument::cetera())->willReturn(null);
        $resource->getResultCode()->willReturn(\Memcached::RES_PARTIAL_READ);
        $resource->getResultMessage()->willReturn('foo');

        $storage = new Cache\Storage\Adapter\Memcached([
            'resource_manager' => $resourceManager->reveal(),
        ]);

        $storage->getEventManager()->attach(
            'getItem.exception',
            function (Cache\Storage\ExceptionEvent $e) {
                $e->setThrowException(false);
                $e->stopPropagation(true);
            },
            -1
        );

        $this->assertNull($storage->getItem('unknwon', $success, $casToken));
        $this->assertFalse($success);
        $this->assertNull($casToken);
    }

    public function getServersDefinitions()
    {
        $expectedServers = [
            ['host' => '127.0.0.1', 'port' => 12345, 'weight' => 1],
            ['host' => 'localhost', 'port' => 54321, 'weight' => 2],
            ['host' => 'examp.com', 'port' => 11211, 'weight' => 0],
        ];

        return [
            // servers as array list
            [
                [
                    ['127.0.0.1', 12345, 1],
                    ['localhost', '54321', '2'],
                    ['examp.com'],
                ],
                $expectedServers,
            ],

            // servers as array assoc
            [
                [
                    ['127.0.0.1', 12345, 1],
                    ['localhost', '54321', '2'],
                    ['examp.com'],
                ],
                $expectedServers,
            ],

            // servers as string list
            [
                [
                    '127.0.0.1:12345?weight=1',
                    'localhost:54321?weight=2',
                    'examp.com',
                ],
                $expectedServers,
            ],

            // servers as string
            [
                '127.0.0.1:12345?weight=1, localhost:54321?weight=2,tcp://examp.com',
                $expectedServers,
            ],
        ];
    }

    /**
     * @dataProvider getServersDefinitions
     */
    public function testOptionSetServers($servers, $expectedServers)
    {
        $options = new Cache\Storage\Adapter\MemcachedOptions();
        $options->setServers($servers);
        $this->assertEquals($expectedServers, $options->getServers());
    }

    public function testLibOptionsSet()
    {
        $options = new Cache\Storage\Adapter\MemcachedOptions();

        $options->setLibOptions([
            'COMPRESSION' => false
        ]);

        $this->assertEquals($options->getResourceManager()->getLibOption(
            $options->getResourceId(),
            \Memcached::OPT_COMPRESSION
        ), false);

        $memcached = new Cache\Storage\Adapter\Memcached($options);
        $this->assertEquals($memcached->getOptions()->getLibOptions(), [
            \Memcached::OPT_COMPRESSION => false
        ]);
    }

    /**
     * @deprecated
     */
    public function testLibOptionSet()
    {
        $options = new Cache\Storage\Adapter\MemcachedOptions();

        $deprecated = false;
        set_error_handler(function () use (& $deprecated) {
            $deprecated = true;
        }, E_USER_DEPRECATED);

        $options->setLibOption('COMPRESSION', false);

        restore_error_handler();
        $this->assertTrue($deprecated, 'Missing deprecated error');

        $this->assertEquals($options->getResourceManager()->getLibOption(
            $options->getResourceId(),
            \Memcached::OPT_COMPRESSION
        ), false);

        $memcached = new Cache\Storage\Adapter\Memcached($options);
        $this->assertEquals($memcached->getOptions()->getLibOptions(), [
                \Memcached::OPT_COMPRESSION => false
        ]);
    }

    public function testOptionPersistentId()
    {
        $options = new Cache\Storage\Adapter\MemcachedOptions();
        $resourceId      = $options->getResourceId();
        $resourceManager = $options->getResourceManager();
        $options->setPersistentId('testPersistentId');

        $this->assertSame('testPersistentId', $resourceManager->getPersistentId($resourceId));
        $this->assertSame('testPersistentId', $options->getPersistentId());
    }

    public function tearDown()
    {
        if ($this->_storage) {
            $this->_storage->flush();
        }

        parent::tearDown();
    }
}

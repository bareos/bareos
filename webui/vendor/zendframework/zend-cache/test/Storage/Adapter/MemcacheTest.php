<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use Zend\Cache;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\Memcache<extended>
 */
class MemcacheTest extends CommonAdapterTest
{
    public function setUp()
    {
        if (getenv('TESTS_ZEND_CACHE_MEMCACHE_ENABLED') != 'true') {
            $this->markTestSkipped('Enable TESTS_ZEND_CACHE_MEMCACHE_ENABLED to run this test');
        }

        if (version_compare('2.0.0', phpversion('memcache')) > 0) {
            try {
                new Cache\Storage\Adapter\Memcache();
                $this->fail("Expected exception Zend\Cache\Exception\ExtensionNotLoadedException");
            } catch (Cache\Exception\ExtensionNotLoadedException $e) {
                $this->markTestSkipped("Missing ext/memcache version >= 2.0.0");
            }
        }

        $this->_options  = new Cache\Storage\Adapter\MemcacheOptions([
            'resource_id' => __CLASS__
        ]);

        if (getenv('TESTS_ZEND_CACHE_MEMCACHE_HOST') && getenv('TESTS_ZEND_CACHE_MEMCACHE_PORT')) {
            $this->_options->getResourceManager()->addServers(__CLASS__, [
                [getenv('TESTS_ZEND_CACHE_MEMCACHE_HOST'), getenv('TESTS_ZEND_CACHE_MEMCACHE_PORT')]
            ]);
        } elseif (getenv('TESTS_ZEND_CACHE_MEMCACHE_HOST')) {
            $this->_options->getResourceManager()->addServers(__CLASS__, [
                [getenv('TESTS_ZEND_CACHE_MEMCACHE_HOST')]
            ]);
        }

        $this->_storage = new Cache\Storage\Adapter\Memcache();
        $this->_storage->setOptions($this->_options);
        $this->_storage->flush();

        parent::setUp();
    }

    public function getCommonAdapterNamesProvider()
    {
        return [
            ['memcache'],
            ['Memcache'],
        ];
    }

    /**
     * Data provider to test valid server info
     *
     * Returns an array of the following structure:
     * array(array(
     *     <array|string server options>,
     *     <array expected normalized servers>,
     * )[, ...])
     *
     * @return array
     */
    public function getServersDefinitions()
    {
        $expectedServers = [
            ['host' => '127.0.0.1', 'port' => 12345, 'weight' => 1, 'status' => true],
            ['host' => 'localhost', 'port' => 54321, 'weight' => 2, 'status' => true],
            ['host' => 'examp.com', 'port' => 11211, 'status' => true],
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
     * @param mixed $servers
     * @param array $expectedServers
     */
    public function testOptionSetServers($servers, $expectedServers)
    {
        $options = new Cache\Storage\Adapter\MemcacheOptions();
        $options->setServers($servers);
        $this->assertEquals($expectedServers, $options->getServers());
    }

    public function testCompressThresholdOptions()
    {
        $threshold = 100;
        $minSavings = 0.2;

        $options = new Cache\Storage\Adapter\MemcacheOptions();
        $options->setAutoCompressThreshold($threshold);
        $options->setAutoCompressMinSavings($minSavings);
        $this->assertEquals(
            $threshold,
            $options->getResourceManager()->getAutoCompressThreshold($options->getResourceId())
        );
        $this->assertEquals(
            $minSavings,
            $options->getResourceManager()->getAutoCompressMinSavings($options->getResourceId())
        );

        $memcache = new Cache\Storage\Adapter\Memcache($options);
        $this->assertEquals($memcache->getOptions()->getAutoCompressThreshold(), $threshold);
        $this->assertEquals($memcache->getOptions()->getAutoCompressMinSavings(), $minSavings);
    }

    public function tearDown()
    {
        if ($this->_storage) {
            $this->_storage->flush();
        }

        parent::tearDown();
    }
}

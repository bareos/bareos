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
use Zend\Cache\Storage\Adapter\MemcacheResourceManager;

/**
 * PHPUnit test case
 */

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\MemcacheResourceManager
 */
class MemcacheResourceManagerTest extends TestCase
{
    /**
     * The resource manager
     *
     * @var MemcacheResourceManager
     */
    protected $resourceManager;

    public function setUp()
    {
        $this->resourceManager = new MemcacheResourceManager();
    }

    /**
     * Data provider to test valid resources
     *
     * Returns an array of the following structure:
     * array(array(
     *     <string resource id>,
     *     <mixed input resource>,
     *     <array normalized server list>
     * )[, ...])
     *
     * @return array
     */
    public function validResourceProvider()
    {
        $data = [
            // empty resource
            [
                'testEmptyResource',
                [],
                [],
            ],

            // servers given as string
            [
                'testServersGivenAsString',
                [
                    'servers' => '127.0.0.1:1234,127.0.0.1,192.1.0.1?weight=3,localhost,127.0.0.1:11211?weight=1' .
                                 ',10.0.0.1:11211?weight=1&status=0&persistent=0&timeout=5&retry_interval=10',
                ],
                [
                    ['host' => '127.0.0.1', 'port' => 1234,  'status' => true],
                    ['host' => '127.0.0.1', 'port' => 11211, 'weight' => 1, 'status' => true],
                    ['host' => '192.1.0.1', 'port' => 11211, 'weight' => 3, 'status' => true],
                    ['host' => 'localhost', 'port' => 11211, 'status' => true],
                    ['host' => '10.0.0.1',  'port' => 11211, 'weight' => 1, 'status' => false,
                          'persistent' => false, 'timeout' => 5,  'retry_interval' => 10],
                ],
            ],

            // servers given as list of strings
            [
                'testServersGivenAsListOfStrings',
                [
                    'servers' => [
                        '127.0.0.1:1234',
                        '127.0.0.1',
                        '192.1.0.1?weight=3',
                        'localhost',
                        '127.0.0.1:11211?weight=1',
                        '10.0.0.1:11211?weight=1&status=0&persistent=0&timeout=5&retry_interval=10',
                    ],
                ],
                [
                    ['host' => '127.0.0.1', 'port' => 1234,  'status' => true],
                    ['host' => '127.0.0.1', 'port' => 11211, 'weight' => 1, 'status' => true],
                    ['host' => '192.1.0.1', 'port' => 11211, 'weight' => 3, 'status' => true],
                    ['host' => 'localhost', 'port' => 11211, 'status' => true],
                    ['host' => '10.0.0.1',  'port' => 11211, 'weight' => 1, 'status' => false,
                          'persistent' => false, 'timeout' => 5,  'retry_interval' => 10],
                ],
            ],

            // servers given as list of arrays
            [
                'testServersGivenAsListOfArrays',
                [
                    'servers' => [
                        ['127.0.0.1', 1234],
                        ['127.0.0.1'],
                        ['192.1.0.1', 11211, 3],
                        ['localhost'],
                        ['127.0.0.1', 11211, 1],
                        ['10.0.0.1',  11211, 1, false, false, 5, 10],
                    ],
                ],
                [
                    ['host' => '127.0.0.1', 'port' => 1234,  'status' => true],
                    ['host' => '127.0.0.1', 'port' => 11211, 'weight' => 1, 'status' => true],
                    ['host' => '192.1.0.1', 'port' => 11211, 'weight' => 3, 'status' => true],
                    ['host' => 'localhost', 'port' => 11211, 'status' => true],
                    ['host' => '10.0.0.1',  'port' => 11211, 'weight' => 1, 'status' => false,
                          'persistent' => false, 'timeout' => 5,  'retry_interval' => 10],
                ],
            ],

            // servers given as list of assoc arrays
            [
                'testServersGivenAsListOfAssocArrays',
                [
                    'servers' => [
                        [
                           'host' => '127.0.0.1',
                           'port' => 1234,
                        ],
                        [
                           'host' => '127.0.0.1',
                        ],
                        [
                            'host'   => '192.1.0.1',
                            'weight' => 3,
                        ],
                        [
                            'host' => 'localhost',
                        ],
                        [
                            'host' => '127.0.0.1',
                            'port' => 11211,
                            'weight' => 1,
                        ],
                        [
                            'host' => '10.0.0.1',
                            'port' => 11211,
                            'weight' => 1,
                            'status' => false,
                            'persistent' => false,
                            'timeout' => 5,
                            'retry_interval' => 10
                        ]
                    ],
                ],
                [
                    ['host' => '127.0.0.1', 'port' => 1234,  'status' => true],
                    ['host' => '127.0.0.1', 'port' => 11211, 'weight' => 1, 'status' => true],
                    ['host' => '192.1.0.1', 'port' => 11211, 'weight' => 3, 'status' => true],
                    ['host' => 'localhost', 'port' => 11211, 'status' => true],
                    ['host' => '10.0.0.1',  'port' => 11211, 'weight' => 1, 'status' => false,
                          'persistent' => false, 'timeout' => 5,  'retry_interval' => 10],
                ],
            ],
        ];

        return $data;
    }

    /**
     * @dataProvider validResourceProvider
     * @param string $resourceId
     * @param mixed  $resource
     * @param array  $expectedServers
     * @param array  $expectedLibOptions
     */
    public function testValidResources($resourceId, $resource, $expectedServers)
    {
        $this->assertSame($this->resourceManager, $this->resourceManager->setResource($resourceId, $resource));
        $this->assertTrue($this->resourceManager->hasResource($resourceId));

        $this->assertEquals($expectedServers, $this->resourceManager->getServers($resourceId));

        $this->assertSame($this->resourceManager, $this->resourceManager->removeResource($resourceId));
        $this->assertFalse($this->resourceManager->hasResource($resourceId));
    }

    /**
     * Data provider to test valid compress threshold options
     *
     * Returns an array of the following structure:
     * array(array(
     *     <string resource id>,
     *     <array threshold options input>,
     *     <array normalized threshold options>
     * )[, ...])
     *
     * @return array
     */
    public function validCompressThresholdOptionsProvider()
    {
        $data = [
            [
                'testThresholdResource',
                [
                    'auto_compress_threshold' => 100,
                ],
                [
                    'auto_compress_threshold' => 100,
                    'auto_compress_min_savings' => null,
                ],
            ],
            [
                'testThresholdAndMinSavingsResource',
                [
                    'auto_compress_threshold' => 100,
                    'auto_compress_min_savings' => 0.2,
                ],
                [
                    'auto_compress_threshold' => 100,
                    'auto_compress_min_savings' => 0.2,
                ],
            ],
            [
                'testStringThresholdAndMinSavingsResource',
                [
                    'auto_compress_threshold' => "100",
                    'auto_compress_min_savings' => "0.2",
                ],
                [
                    'auto_compress_threshold' => 100,
                    'auto_compress_min_savings' => 0.2,
                ],
            ],
            [
                'testThresholdArrayResource',
                [
                    'auto_compress_threshold' => [
                        'threshold' => 100,
                        'min_savings' => 0.2,
                    ],
                ],
                [
                    'auto_compress_threshold' => 100,
                    'auto_compress_min_savings' => 0.2,
                ],
            ],
        ];
        return $data;
    }

    /**
     * @dataProvider validCompressThresholdOptionsProvider
     * @param string $resourceId
     * @param array $thresholdOptions
     * @param array $expectedOptions
     */
    public function testSetCompressThreshold($resourceId, $thresholdOptions, $expectedOptions)
    {
        // Test normalized values
        $this->resourceManager->setResource($resourceId, $thresholdOptions);
        $this->assertEquals(
            $expectedOptions['auto_compress_threshold'],
            $this->resourceManager->getAutoCompressThreshold($resourceId)
        );
        $this->assertEquals(
            $expectedOptions['auto_compress_min_savings'],
            $this->resourceManager->getAutoCompressMinSavings($resourceId)
        );

        // Test memcache set
        $resourceMock = $this->getMockBuilder('Memcache')
            ->setMethods(['setCompressThreshold'])
            ->getMock();
        if (isset($thresholdOptions['auto_compress_min_savings'])
            && $thresholdOptions['auto_compress_min_savings'] !== null
        ) {
            $resourceMock
                ->expects($this->once())
                ->method('setCompressThreshold')
                ->with(
                    $this->equalTo($expectedOptions['auto_compress_threshold']),
                    $this->equalTo($expectedOptions['auto_compress_min_savings'])
                );
        } else {
            $resourceMock
                ->expects($this->once())
                ->method('setCompressThreshold')
                ->with($this->equalTo($expectedOptions['auto_compress_threshold']));
        }

        $this->resourceManager->setResource($resourceId, $resourceMock);
        if (isset($thresholdOptions['auto_compress_min_savings'])) {
            $this->resourceManager->setAutoCompressThreshold(
                $resourceId,
                $thresholdOptions['auto_compress_threshold'],
                $thresholdOptions['auto_compress_min_savings']
            );
        } else {
            $this->resourceManager->setAutoCompressThreshold(
                $resourceId,
                $thresholdOptions['auto_compress_threshold']
            );
        }

        // After create test
        $this->expectException('Zend\Cache\Exception\RuntimeException');
        $this->expectExceptionMessage('Cannot get compress threshold once resource is created');
        $this->assertEquals(
            $expectedOptions['auto_compress_threshold'],
            $this->resourceManager->getAutoCompressThreshold($resourceId)
        );
    }

    /**
     * Data provider to test valid server info
     *
     * Returns an array of the following structure:
     * array(array(
     *     <string resource id>,
     *     <array server options>,
     *     <array server defaults>,
     *     <array expected memcache addServer arguments>,
     * )[, ...])
     *
     * @return array
     */
    public function validServerAndServerDefaultsProvider()
    {
        $data = [
            // All params, no default settings
            [
                'testServerAllParamsNoDefaults',
                [
                    'host' => '10.0.0.1',  'port' => 11211, 'weight' => 2, 'status' => false,
                    'persistent' => false, 'timeout' => 5,  'retry_interval' => 10,
                ],
                [],
                [
                    'host' => '10.0.0.1',  'port' => 11211, 'weight' => 2, 'status' => false,
                    'persistent' => false, 'timeout' => 5,  'retry_interval' => 10,
                ],
            ],
            // Default settings
            [
                'testServerWithDefaults',
                [
                    'host' => '10.0.0.1',  'port' => 11211,
                ],
                [],
                [
                    'host' => '10.0.0.1',  'port' => 11211, 'weight' => 1, 'status' => true,
                    'persistent' => true, 'timeout' => 1,  'retry_interval' => 15,
                ],
            ],
            // Custom default settings
            [
                'testServerWithCustomDefaults',
                [
                    'host' => '10.0.0.1',  'port' => 11211, 'status' => false,
                ],
                ['persistent' => false, 'timeout' => 5,  'retry_interval' => 10, 'weight' => 3],
                [
                    'host' => '10.0.0.1',  'port' => 11211, 'weight' => 3, 'status' => false,
                    'persistent' => false, 'timeout' => 5,  'retry_interval' => 10,
                ],
            ],
        ];
        return $data;
    }

    /**
     * @dataProvider validServerAndServerDefaultsProvider
     * @param string $resourceId
     * @param array  $server
     * @param array  $serverDefaults
     * @param array  $expectedParams
     */
    public function testAddServerOnExistingResource($resourceId, $server, $serverDefaults, $expectedParams)
    {
        $resourceMock = $this->getMockBuilder('Memcache')
            ->setMethods(['addServer'])
            ->getMock();
        $resourceMock
            ->expects($this->once())
            ->method('addServer')
            ->with(
                $this->equalTo($expectedParams['host']),
                $this->equalTo($expectedParams['port']),
                $this->equalTo($expectedParams['persistent']),
                $this->equalTo($expectedParams['weight']),
                $this->equalTo($expectedParams['timeout']),
                $this->equalTo($expectedParams['retry_interval']),
                $this->equalTo($expectedParams['status'])
            );

        $this->resourceManager->setResource($resourceId, $resourceMock, null, $serverDefaults);
        $this->resourceManager->addServer($resourceId, $server);
    }
}

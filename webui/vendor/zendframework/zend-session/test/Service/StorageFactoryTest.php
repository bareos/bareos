<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Session\Service;

use ArrayObject;
use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;
use Zend\ServiceManager\ServiceManager;
use Zend\Session\Service\StorageFactory;
use Zend\Session\Storage\ArrayStorage;
use Zend\Session\Storage\SessionArrayStorage;
use Zend\Session\Storage\StorageInterface;

/**
 * @group      Zend_Session
 * @covers Zend\Session\Service\StorageFactory
 */
class StorageFactoryTest extends TestCase
{
    public function setUp()
    {
        $config = new Config([
            'factories' => [
                StorageInterface::class => StorageFactory::class,
            ],
        ]);
        $this->services = new ServiceManager();
        $config->configureServiceManager($this->services);
    }

    public function sessionStorageConfig()
    {
        return [
            'array-storage-short' => [[
                'session_storage' => [
                    'type' => 'ArrayStorage',
                    'options' => [
                        'input' => [
                            'foo' => 'bar',
                        ],
                    ],
                ],
            ], ArrayStorage::class],
            'array-storage-fqcn' => [[
                'session_storage' => [
                    'type' => ArrayStorage::class,
                    'options' => [
                        'input' => [
                            'foo' => 'bar',
                        ],
                    ],
                ],
            ], ArrayStorage::class],
            'session-array-storage-short' => [[
                'session_storage' => [
                    'type' => 'SessionArrayStorage',
                    'options' => [
                        'input' => [
                            'foo' => 'bar',
                        ],
                    ],
                ],
            ], SessionArrayStorage::class],
            'session-array-storage-arrayobject' => [[
                'session_storage' => [
                    'type' => 'SessionArrayStorage',
                    'options' => [
                        'input' => new ArrayObject([
                            'foo' => 'bar',
                        ]),
                    ],
                ],
            ], SessionArrayStorage::class],
            'session-array-storage-fqcn' => [[
                'session_storage' => [
                    'type' => SessionArrayStorage::class,
                    'options' => [
                        'input' => [
                            'foo' => 'bar',
                        ],
                    ],
                ],
            ], SessionArrayStorage::class],
        ];
    }

    /**
     * @dataProvider sessionStorageConfig
     */
    public function testUsesConfigurationToCreateStorage($config, $class)
    {
        $this->services->setService('config', $config);
        $storage = $this->services->get(StorageInterface::class);
        $this->assertInstanceOf($class, $storage);
        $test = $storage->toArray();
        $this->assertEquals($config['session_storage']['options']['input'], $test);
    }

    public function testConfigurationWithoutInputIsValid()
    {
        $this->services->setService('config', [
            'session_storage' => [
                'type' => SessionArrayStorage::class,
                'options' => [],
            ],
        ]);

        $storage = $this->services->get(StorageInterface::class);

        $this->assertInstanceOf(SessionArrayStorage::class, $storage);
        $this->assertSame([], $storage->toArray());
    }

    public function invalidSessionStorageConfig()
    {
        return [
            'unknown-class-short' => [[
                'session_storage' => [
                    'type' => 'FooStorage',
                    'options' => [],
                ],
            ]],
            'unknown-class-fqcn' => [[
                'session_storage' => [
                    'type' => 'Foo\Bar\Baz\Bat',
                    'options' => [],
                ],
            ]],
            'bad-class' => [[
                'session_storage' => [
                    'type' => 'Zend\Session\Config\StandardConfig',
                    'options' => [],
                ],
            ]],
            'good-class-invalid-options' => [[
                'session_storage' => [
                    'type' => 'ArrayStorage',
                    'options' => [
                        'input' => 'this is invalid',
                    ],
                ],
            ]],
        ];
    }

    /**
     * @dataProvider invalidSessionStorageConfig
     */
    public function testInvalidConfigurationRaisesServiceNotCreatedException($config)
    {
        $this->services->setService('config', $config);
        $this->expectException(ServiceNotCreatedException::class);
        $storage = $this->services->get(StorageInterface::class);
    }
}

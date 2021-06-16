<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache;

use PHPUnit\Framework\TestCase;
use Zend\Cache;
use Zend\ServiceManager\ServiceManager;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\StorageFactory
 */
class StorageFactoryTest extends TestCase
{
    public function setUp()
    {
        Cache\StorageFactory::resetAdapterPluginManager();
        Cache\StorageFactory::resetPluginManager();
    }

    public function tearDown()
    {
        Cache\StorageFactory::resetAdapterPluginManager();
        Cache\StorageFactory::resetPluginManager();
    }

    public function testDefaultAdapterPluginManager()
    {
        $adapters = Cache\StorageFactory::getAdapterPluginManager();
        $this->assertInstanceOf('Zend\Cache\Storage\AdapterPluginManager', $adapters);
    }

    public function testChangeAdapterPluginManager()
    {
        $adapters = new Cache\Storage\AdapterPluginManager(new ServiceManager);
        Cache\StorageFactory::setAdapterPluginManager($adapters);
        $this->assertSame($adapters, Cache\StorageFactory::getAdapterPluginManager());
    }

    public function testAdapterFactory()
    {
        $adapter1 = Cache\StorageFactory::adapterFactory('Memory');
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $adapter1);

        $adapter2 = Cache\StorageFactory::adapterFactory('Memory');
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $adapter2);

        $this->assertNotSame($adapter1, $adapter2);
    }

    public function testDefaultPluginManager()
    {
        $manager = Cache\StorageFactory::getPluginManager();
        $this->assertInstanceOf('Zend\Cache\Storage\PluginManager', $manager);
    }

    public function testChangePluginManager()
    {
        $manager = new Cache\Storage\PluginManager(new ServiceManager);
        Cache\StorageFactory::setPluginManager($manager);
        $this->assertSame($manager, Cache\StorageFactory::getPluginManager());
    }

    public function testPluginFactory()
    {
        $plugin1 = Cache\StorageFactory::pluginFactory('Serializer');
        $this->assertInstanceOf('Zend\Cache\Storage\Plugin\Serializer', $plugin1);

        $plugin2 = Cache\StorageFactory::pluginFactory('Serializer');
        $this->assertInstanceOf('Zend\Cache\Storage\Plugin\Serializer', $plugin2);

        $this->assertNotSame($plugin1, $plugin2);
    }

    public function testFactoryAdapterAsString()
    {
        $cache = Cache\StorageFactory::factory([
            'adapter' => 'Memory',
        ]);
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $cache);
    }

    /**
     * @group 4445
     */
    public function testFactoryWithAdapterAsStringAndOptions()
    {
        $cache = Cache\StorageFactory::factory([
            'adapter' => 'Memory',
            'options' => [
                'namespace' => 'test'
            ],
        ]);

        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $cache);
        $this->assertSame('test', $cache->getOptions()->getNamespace());
    }

    public function testFactoryAdapterAsArray()
    {
        $cache = Cache\StorageFactory::factory([
            'adapter' => [
                'name' => 'Memory',
            ]
        ]);
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $cache);
    }

    public function testFactoryWithPlugins()
    {
        $adapter = 'Memory';
        $plugins = ['Serializer', 'ClearExpiredByFactor'];

        $cache = Cache\StorageFactory::factory([
            'adapter' => $adapter,
            'plugins' => $plugins,
        ]);

        // test adapter
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\Memory', $cache);

        // test plugin structure
        $i = 0;
        foreach ($cache->getPluginRegistry() as $plugin) {
            $this->assertInstanceOf('Zend\Cache\Storage\Plugin\\' . $plugins[$i++], $plugin);
        }
    }

    public function testFactoryInstantiateAdapterWithPluginsWithoutEventsCapableInterfaceThrowsException()
    {
        // The BlackHole adapter doesn't implement EventsCapableInterface
        $this->expectException('Zend\Cache\Exception\RuntimeException');
        Cache\StorageFactory::factory([
            'adapter' => 'blackhole',
            'plugins' => ['Serializer'],
        ]);
    }

    public function testFactoryWithPluginsAndOptionsArray()
    {
        $factory = [
            'adapter' => [
                 'name' => 'Memory',
                 'options' => [
                     'ttl' => 123,
                     'namespace' => 'willBeOverwritten'
                 ],
            ],
            'plugins' => [
                // plugin as a simple string entry
                'Serializer',

                // plugin as name-options pair
                'ClearExpiredByFactor' => [
                    'clearing_factor' => 1,
                ],

                // plugin with full definition
                [
                    'name'     => 'IgnoreUserAbort',
                    'priority' => 100,
                    'options'  => [
                        'exit_on_abort' => false,
                    ],
                ],
            ],
            'options' => [
                'namespace' => 'test',
            ]
        ];
        $storage = Cache\StorageFactory::factory($factory);

        // test adapter
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\\' . $factory['adapter']['name'], $storage);
        $this->assertEquals(123, $storage->getOptions()->getTtl());
        $this->assertEquals('test', $storage->getOptions()->getNamespace());

        // test plugin structure
        foreach ($storage->getPluginRegistry() as $plugin) {
            // test plugin options
            $pluginClass = get_class($plugin);
            switch ($pluginClass) {
                case 'Zend\Cache\Storage\Plugin\ClearExpiredByFactor':
                    $this->assertSame(
                        $factory['plugins']['ClearExpiredByFactor']['clearing_factor'],
                        $plugin->getOptions()->getClearingFactor()
                    );
                    break;

                case 'Zend\Cache\Storage\Plugin\Serializer':
                    break;

                case 'Zend\Cache\Storage\Plugin\IgnoreUserAbort':
                    $this->assertFalse($plugin->getOptions()->getExitOnAbort());
                    break;

                default:
                    $this->fail("Unexpected plugin class '{$pluginClass}'");
            }
        }
    }
}

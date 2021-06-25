<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Service;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Prophecy\Argument;
use Zend\Cache\Service\StorageCacheAbstractServiceFactory;
use Zend\Cache\StorageFactory;
use Zend\Cache\Storage\AdapterPluginManager;
use Zend\Cache\Storage\Adapter\AbstractAdapter;
use Zend\Cache\Storage\Adapter\Memory;
use Zend\Cache\Storage\PluginManager;
use Zend\Cache\Storage\Plugin\PluginInterface;
use Zend\Cache\Storage\StorageInterface;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;

/**
 * @covers Zend\Cache\StorageFactory<extended>
 */
class StorageCacheAbstractServiceFactoryTest extends TestCase
{
    protected $sm;

    public function setUp()
    {
        StorageFactory::resetAdapterPluginManager();
        StorageFactory::resetPluginManager();
        $config = [
            'services' => [
                'config' => [
                    'caches' => [
                        'Memory' => [
                            'adapter' => 'Memory',
                            'plugins' => ['Serializer', 'ClearExpiredByFactor'],
                        ],
                        'Foo' => [
                            'adapter' => 'Memory',
                            'plugins' => ['Serializer', 'ClearExpiredByFactor'],
                        ],
                    ]
                ],
            ],
            'abstract_factories' => [
                StorageCacheAbstractServiceFactory::class
            ]
        ];
        $this->sm = new ServiceManager();
        (new Config($config))->configureServiceManager($this->sm);
    }

    public function tearDown()
    {
        StorageFactory::resetAdapterPluginManager();
        StorageFactory::resetPluginManager();
    }

    public function testCanLookupCacheByName()
    {
        $this->assertTrue($this->sm->has('Memory'));
        $this->assertTrue($this->sm->has('Foo'));
    }

    public function testCanRetrieveCacheByName()
    {
        $cacheA = $this->sm->get('Memory');
        $this->assertInstanceOf(Memory::class, $cacheA);

        $cacheB = $this->sm->get('Foo');
        $this->assertInstanceOf(Memory::class, $cacheB);

        $this->assertNotSame($cacheA, $cacheB);
    }

    public function testInvalidCacheServiceNameWillBeIgnored()
    {
        $this->assertFalse($this->sm->has('invalid'));
    }

    public function testSetsFactoryAdapterPluginManagerInstanceOnInvocation()
    {
        $adapter = $this->prophesize(AbstractAdapter::class);
        $adapter->willImplement(StorageInterface::class);
        $adapter->setOptions(Argument::any())->shouldNotBeCalled();
        $adapter->hasPlugin(Argument::any(), Argument::any())->shouldNotBeCalled();
        $adapter->addPlugin(Argument::any(), Argument::any())->shouldNotBeCalled();

        $adapterPluginManager = $this->prophesize(AdapterPluginManager::class);
        $adapterPluginManager->get('Memory')->willReturn($adapter->reveal());

        $container = $this->prophesize(ContainerInterface::class);
        $container->has(AdapterPluginManager::class)->willReturn(true);
        $container->get(AdapterPluginManager::class)->willReturn($adapterPluginManager->reveal());
        $container->has(PluginManager::class)->willReturn(false);

        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'caches' => [ 'Cache' => [ 'adapter' => 'Memory' ]],
        ]);

        $factory = new StorageCacheAbstractServiceFactory();
        $this->assertSame($adapter->reveal(), $factory($container->reveal(), 'Cache'));
        $this->assertSame($adapterPluginManager->reveal(), StorageFactory::getAdapterPluginManager());
    }

    public function testSetsFactoryPluginManagerInstanceOnInvocation()
    {
        $plugin = $this->prophesize(PluginInterface::class);
        $plugin->setOptions(Argument::any())->shouldNotBeCalled();

        $pluginManager = $this->prophesize(PluginManager::class);
        $pluginManager->get('Serializer')->willReturn($plugin->reveal());

        $adapter = $this->prophesize(AbstractAdapter::class);
        $adapter->willImplement(StorageInterface::class);
        $adapter->setOptions(Argument::any())->shouldNotBeCalled();
        $adapter->hasPlugin($plugin->reveal(), Argument::any())->willReturn(false);
        $adapter->addPlugin($plugin->reveal(), Argument::any())->shouldBeCalled();

        $adapterPluginManager = $this->prophesize(AdapterPluginManager::class);
        $adapterPluginManager->get('Memory')->willReturn($adapter->reveal());

        $container = $this->prophesize(ContainerInterface::class);
        $container->has(AdapterPluginManager::class)->willReturn(true);
        $container->get(AdapterPluginManager::class)->willReturn($adapterPluginManager->reveal());
        $container->has(PluginManager::class)->willReturn(true);
        $container->get(PluginManager::class)->willReturn($pluginManager->reveal());

        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'caches' => [ 'Cache' => [
                'adapter' => 'Memory',
                'plugins' => ['Serializer'],
            ]],
        ]);

        $factory = new StorageCacheAbstractServiceFactory();
        $factory($container->reveal(), 'Cache');
        $this->assertSame($pluginManager->reveal(), StorageFactory::getPluginManager());
    }
}

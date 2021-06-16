<?php
/**
 * @link      http://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Service;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\StorageInterface;
use Zend\Cache\Storage\AdapterPluginManager;
use Zend\Cache\Service\StorageAdapterPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class StorageAdapterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new StorageAdapterPluginManagerFactory();

        $adapters = $factory($container, AdapterPluginManager::class);
        $this->assertInstanceOf(AdapterPluginManager::class, $adapters);

        if (method_exists($adapters, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $adapters);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $adapters->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $adapter = $this->prophesize(StorageInterface::class)->reveal();

        $factory = new StorageAdapterPluginManagerFactory();
        $adapters = $factory($container, AdapterPluginManager::class, [
            'services' => [
                'test' => $adapter,
            ],
        ]);
        $this->assertSame($adapter, $adapters->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $adapter = $this->prophesize(StorageInterface::class)->reveal();

        $factory = new StorageAdapterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $adapter,
            ],
        ]);

        $adapters = $factory->createService($container->reveal());
        $this->assertSame($adapter, $adapters->get('test'));
    }
}

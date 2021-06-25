<?php
/**
 * @link      http://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Service;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\Plugin\PluginInterface;
use Zend\Cache\Storage\PluginManager;
use Zend\Cache\Service\StoragePluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class StoragePluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new StoragePluginManagerFactory();

        $plugins = $factory($container, PluginManager::class);
        $this->assertInstanceOf(PluginManager::class, $plugins);

        if (method_exists($plugins, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $plugins);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $plugins->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $plugin = $this->prophesize(pluginInterface::class)->reveal();

        $factory = new StoragePluginManagerFactory();
        $plugins = $factory($container, PluginManager::class, [
            'services' => [
                'test' => $plugin,
            ],
        ]);
        $this->assertSame($plugin, $plugins->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $plugin = $this->prophesize(PluginInterface::class)->reveal();

        $factory = new StoragePluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $plugin,
            ],
        ]);

        $plugins = $factory->createService($container->reveal());
        $this->assertSame($plugin, $plugins->get('test'));
    }
}

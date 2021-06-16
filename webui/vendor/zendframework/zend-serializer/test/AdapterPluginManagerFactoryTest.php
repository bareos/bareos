<?php
/**
 * @link      http://github.com/zendframework/zend-serializer for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Serializer;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Serializer\Adapter\AdapterInterface;
use Zend\Serializer\AdapterPluginManager;
use Zend\Serializer\AdapterPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class AdapterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new AdapterPluginManagerFactory();

        $serializers = $factory($container, AdapterPluginManagerFactory::class);
        $this->assertInstanceOf(AdapterPluginManager::class, $serializers);

        if (method_exists($serializers, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $serializers);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $serializers->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $serializer = $this->prophesize(AdapterInterface::class)->reveal();

        $factory = new AdapterPluginManagerFactory();
        $serializers = $factory($container, AdapterPluginManagerFactory::class, [
            'services' => [
                'test' => $serializer,
            ],
        ]);
        $this->assertSame($serializer, $serializers->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $serializer = $this->prophesize(AdapterInterface::class)->reveal();

        $factory = new AdapterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $serializer,
            ],
        ]);

        $serializers = $factory->createService($container->reveal());
        $this->assertSame($serializer, $serializers->get('test'));
    }
}

<?php
/**
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Log\Writer\WriterInterface;
use Zend\Log\WriterPluginManager;
use Zend\Log\WriterPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class WriterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new WriterPluginManagerFactory();

        $writers = $factory($container, WriterPluginManagerFactory::class);
        $this->assertInstanceOf(WriterPluginManager::class, $writers);

        if (method_exists($writers, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $writers);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $writers->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $writer = $this->prophesize(WriterInterface::class)->reveal();

        $factory = new WriterPluginManagerFactory();
        $writers = $factory($container, WriterPluginManagerFactory::class, [
            'services' => [
                'test' => $writer,
            ],
        ]);
        $this->assertSame($writer, $writers->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $writer = $this->prophesize(WriterInterface::class)->reveal();

        $factory = new WriterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $writer,
            ],
        ]);

        $writers = $factory->createService($container->reveal());
        $this->assertSame($writer, $writers->get('test'));
    }

    public function testConfiguresWriterServicesWhenFound()
    {
        $writer = $this->prophesize(WriterInterface::class)->reveal();
        $config = [
            'log_writers' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($writer) {
                        return $writer;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn($config);

        $factory = new WriterPluginManagerFactory();
        $writers = $factory($container->reveal(), 'WriterManager');

        $this->assertInstanceOf(WriterPluginManager::class, $writers);
        $this->assertTrue($writers->has('test'));
        $this->assertSame($writer, $writers->get('test'));
        $this->assertTrue($writers->has('test-too'));
        $this->assertSame($writer, $writers->get('test-too'));
    }

    public function testDoesNotConfigureWriterServicesWhenServiceListenerPresent()
    {
        $writer = $this->prophesize(WriterInterface::class)->reveal();
        $config = [
            'log_writers' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($writer) {
                        return $writer;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(true);
        $container->has('config')->shouldNotBeCalled();
        $container->get('config')->shouldNotBeCalled();

        $factory = new WriterPluginManagerFactory();
        $writers = $factory($container->reveal(), 'WriterManager');

        $this->assertInstanceOf(WriterPluginManager::class, $writers);
        $this->assertFalse($writers->has('test'));
        $this->assertFalse($writers->has('test-too'));
    }

    public function testDoesNotConfigureWriterServicesWhenConfigServiceNotPresent()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(false);
        $container->get('config')->shouldNotBeCalled();

        $factory = new WriterPluginManagerFactory();
        $writers = $factory($container->reveal(), 'WriterManager');

        $this->assertInstanceOf(WriterPluginManager::class, $writers);
    }

    public function testDoesNotConfigureWriterServicesWhenConfigServiceDoesNotContainWritersConfig()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn(['foo' => 'bar']);

        $factory = new WriterPluginManagerFactory();
        $writers = $factory($container->reveal(), 'WriterManager');

        $this->assertInstanceOf(WriterPluginManager::class, $writers);
        $this->assertFalse($writers->has('foo'));
    }
}

<?php
/**
 * @link      http://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\InputFilter\InputFilterInterface;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\InputFilter\InputFilterPluginManagerFactory;
use Zend\InputFilter\InputInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class InputFilterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new InputFilterPluginManagerFactory();

        $filters = $factory($container, InputFilterPluginManagerFactory::class);
        $this->assertInstanceOf(InputFilterPluginManager::class, $filters);

        if (method_exists($filters, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $filters);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $filters->getServiceLocator());
        }
    }

    public function pluginProvider()
    {
        return [
            'input' => [InputInterface::class],
            'input-filter' => [InputFilterInterface::class],
        ];
    }

    /**
     * @depends testFactoryReturnsPluginManager
     * @dataProvider pluginProvider
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop($pluginType)
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $plugin = $this->prophesize($pluginType)->reveal();

        $factory = new InputFilterPluginManagerFactory();
        $filters = $factory($container, InputFilterPluginManagerFactory::class, [
            'services' => [
                'test' => $plugin,
            ],
        ]);
        $this->assertSame($plugin, $filters->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     * @dataProvider pluginProvider
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2($pluginType)
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $plugin = $this->prophesize($pluginType)->reveal();

        $factory = new InputFilterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $plugin,
            ],
        ]);

        $filters = $factory->createService($container->reveal());
        $this->assertSame($plugin, $filters->get('test'));
    }

    public function testConfiguresInputFilterServicesWhenFound()
    {
        $inputFilter = $this->prophesize(InputFilterInterface::class)->reveal();
        $config = [
            'input_filters' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($inputFilter) {
                        return $inputFilter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn($config);

        $factory = new InputFilterPluginManagerFactory();
        $inputFilters = $factory($container->reveal(), 'InputFilterManager');

        $this->assertInstanceOf(InputFilterPluginManager::class, $inputFilters);
        $this->assertTrue($inputFilters->has('test'));
        $this->assertSame($inputFilter, $inputFilters->get('test'));
        $this->assertTrue($inputFilters->has('test-too'));
        $this->assertSame($inputFilter, $inputFilters->get('test-too'));
    }

    public function testDoesNotConfigureInputFilterServicesWhenServiceListenerPresent()
    {
        $inputFilter = $this->prophesize(InputFilterInterface::class)->reveal();
        $config = [
            'input_filters' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($inputFilter) {
                        return $inputFilter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(true);
        $container->has('config')->shouldNotBeCalled();
        $container->get('config')->shouldNotBeCalled();

        $factory = new InputFilterPluginManagerFactory();
        $inputFilters = $factory($container->reveal(), 'InputFilterManager');

        $this->assertInstanceOf(InputFilterPluginManager::class, $inputFilters);
        $this->assertFalse($inputFilters->has('test'));
        $this->assertFalse($inputFilters->has('test-too'));
    }

    public function testDoesNotConfigureInputFilterServicesWhenConfigServiceNotPresent()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(false);
        $container->get('config')->shouldNotBeCalled();

        $factory = new InputFilterPluginManagerFactory();
        $inputFilters = $factory($container->reveal(), 'InputFilterManager');

        $this->assertInstanceOf(InputFilterPluginManager::class, $inputFilters);
    }

    public function testDoesNotConfigureInputFilterServicesWhenConfigServiceDoesNotContainInputFiltersConfig()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn(['foo' => 'bar']);

        $factory = new InputFilterPluginManagerFactory();
        $inputFilters = $factory($container->reveal(), 'InputFilterManager');

        $this->assertInstanceOf(InputFilterPluginManager::class, $inputFilters);
        $this->assertFalse($inputFilters->has('foo'));
    }
}

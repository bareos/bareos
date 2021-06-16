<?php
/**
 * @link      http://github.com/zendframework/zend-filter for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Filter\Boolean;
use Zend\Filter\FilterInterface;
use Zend\Filter\FilterPluginManager;
use Zend\Filter\FilterPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class FilterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new FilterPluginManagerFactory();

        $filters = $factory($container, FilterPluginManagerFactory::class);
        $this->assertInstanceOf(FilterPluginManager::class, $filters);

        if (method_exists($filters, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $filters);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $filters->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $filter = function ($value) {
            return $value;
        };

        $factory = new FilterPluginManagerFactory();
        $filters = $factory($container, FilterPluginManagerFactory::class, [
            'services' => [
                'test' => $filter,
            ],
        ]);
        $this->assertSame($filter, $filters->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $filter = function ($value) {
            return $value;
        };

        $factory = new FilterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $filter,
            ],
        ]);

        $filters = $factory->createService($container->reveal());
        $this->assertSame($filter, $filters->get('test'));
    }

    public function testConfiguresFilterServicesWhenFound()
    {
        $filter = $this->prophesize(FilterInterface::class)->reveal();
        $config = [
            'filters' => [
                'aliases' => [
                    'test' => Boolean::class,
                ],
                'factories' => [
                    'test-too' => function ($container) use ($filter) {
                        return $filter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn($config);

        $factory = new FilterPluginManagerFactory();
        $filters = $factory($container->reveal(), 'FilterManager');

        $this->assertInstanceOf(FilterPluginManager::class, $filters);
        $this->assertTrue($filters->has('test'));
        $this->assertInstanceOf(Boolean::class, $filters->get('test'));
        $this->assertTrue($filters->has('test-too'));
        $this->assertSame($filter, $filters->get('test-too'));
    }

    public function testDoesNotConfigureFilterServicesWhenServiceListenerPresent()
    {
        $filter = $this->prophesize(FilterInterface::class)->reveal();
        $config = [
            'filters' => [
                'aliases' => [
                    'test' => Boolean::class,
                ],
                'factories' => [
                    'test-too' => function ($container) use ($filter) {
                        return $filter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(true);
        $container->has('config')->shouldNotBeCalled();
        $container->get('config')->shouldNotBeCalled();

        $factory = new FilterPluginManagerFactory();
        $filters = $factory($container->reveal(), 'FilterManager');

        $this->assertInstanceOf(FilterPluginManager::class, $filters);
        $this->assertFalse($filters->has('test'));
        $this->assertFalse($filters->has('test-too'));
    }

    public function testDoesNotConfigureFilterServicesWhenConfigServiceNotPresent()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(false);
        $container->get('config')->shouldNotBeCalled();

        $factory = new FilterPluginManagerFactory();
        $filters = $factory($container->reveal(), 'FilterManager');

        $this->assertInstanceOf(FilterPluginManager::class, $filters);
    }

    public function testDoesNotConfigureFilterServicesWhenConfigServiceDoesNotContainFiltersConfig()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn(['foo' => 'bar']);

        $factory = new FilterPluginManagerFactory();
        $filters = $factory($container->reveal(), 'FilterManager');

        $this->assertInstanceOf(FilterPluginManager::class, $filters);
        $this->assertFalse($filters->has('foo'));
    }
}

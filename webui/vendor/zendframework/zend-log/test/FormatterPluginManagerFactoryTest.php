<?php
/**
 * @link      http://github.com/zendframework/zend-log for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Log\Formatter\FormatterInterface;
use Zend\Log\FormatterPluginManager;
use Zend\Log\FormatterPluginManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class FormatterPluginManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new FormatterPluginManagerFactory();

        $formatters = $factory($container, FormatterPluginManagerFactory::class);
        $this->assertInstanceOf(FormatterPluginManager::class, $formatters);

        if (method_exists($formatters, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $formatters);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $formatters->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $formatter = $this->prophesize(FormatterInterface::class)->reveal();

        $factory = new FormatterPluginManagerFactory();
        $formatters = $factory($container, FormatterPluginManagerFactory::class, [
            'services' => [
                'test' => $formatter,
            ],
        ]);
        $this->assertSame($formatter, $formatters->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $formatter = $this->prophesize(FormatterInterface::class)->reveal();

        $factory = new FormatterPluginManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $formatter,
            ],
        ]);

        $formatters = $factory->createService($container->reveal());
        $this->assertSame($formatter, $formatters->get('test'));
    }

    public function testConfiguresFormatterServicesWhenFound()
    {
        $formatter = $this->prophesize(FormatterInterface::class)->reveal();
        $config = [
            'log_formatters' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($formatter) {
                        return $formatter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn($config);

        $factory = new FormatterPluginManagerFactory();
        $formatters = $factory($container->reveal(), 'FormatterManager');

        $this->assertInstanceOf(FormatterPluginManager::class, $formatters);
        $this->assertTrue($formatters->has('test'));
        $this->assertSame($formatter, $formatters->get('test'));
        $this->assertTrue($formatters->has('test-too'));
        $this->assertSame($formatter, $formatters->get('test-too'));
    }

    public function testDoesNotConfigureFormatterServicesWhenServiceListenerPresent()
    {
        $formatter = $this->prophesize(FormatterInterface::class)->reveal();
        $config = [
            'log_formatters' => [
                'aliases' => [
                    'test' => 'test-too',
                ],
                'factories' => [
                    'test-too' => function ($container) use ($formatter) {
                        return $formatter;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(true);
        $container->has('config')->shouldNotBeCalled();
        $container->get('config')->shouldNotBeCalled();

        $factory = new FormatterPluginManagerFactory();
        $formatters = $factory($container->reveal(), 'FormatterManager');

        $this->assertInstanceOf(FormatterPluginManager::class, $formatters);
        $this->assertFalse($formatters->has('test'));
        $this->assertFalse($formatters->has('test-too'));
    }

    public function testDoesNotConfigureFormatterServicesWhenConfigServiceNotPresent()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(false);
        $container->get('config')->shouldNotBeCalled();

        $factory = new FormatterPluginManagerFactory();
        $formatters = $factory($container->reveal(), 'FormatterManager');

        $this->assertInstanceOf(FormatterPluginManager::class, $formatters);
    }

    public function testDoesNotConfigureFormatterServicesWhenConfigServiceDoesNotContainFormattersConfig()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn(['foo' => 'bar']);

        $factory = new FormatterPluginManagerFactory();
        $formatters = $factory($container->reveal(), 'FormatterManager');

        $this->assertInstanceOf(FormatterPluginManager::class, $formatters);
        $this->assertFalse($formatters->has('foo'));
    }
}

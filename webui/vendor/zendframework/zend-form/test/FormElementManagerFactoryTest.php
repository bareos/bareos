<?php
/**
 * @link      http://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Form\ElementInterface;
use Zend\Form\Element\Number;
use Zend\Form\FormElementManager;
use Zend\Form\FormElementManagerFactory;
use Zend\ServiceManager\ServiceLocatorInterface;

class FormElementManagerFactoryTest extends TestCase
{
    public function testFactoryReturnsPluginManager()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $factory = new FormElementManagerFactory();

        $elements = $factory($container, FormElementManager::class);
        $this->assertInstanceOf(FormElementManager::class, $elements);

        if (method_exists($elements, 'configure')) {
            // zend-servicemanager v3
            $this->assertAttributeSame($container, 'creationContext', $elements);
        } else {
            // zend-servicemanager v2
            $this->assertSame($container, $elements->getServiceLocator());
        }
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderContainerInterop()
    {
        $container = $this->prophesize(ContainerInterface::class)->reveal();
        $element = $this->prophesize(ElementInterface::class)->reveal();

        $factory = new FormElementManagerFactory();
        $elements = $factory($container, FormElementManager::class, [
            'services' => [
                'test' => $element,
            ],
        ]);
        $this->assertSame($element, $elements->get('test'));
    }

    /**
     * @depends testFactoryReturnsPluginManager
     */
    public function testFactoryConfiguresPluginManagerUnderServiceManagerV2()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $element = $this->prophesize(ElementInterface::class)->reveal();

        $factory = new FormElementManagerFactory();
        $factory->setCreationOptions([
            'services' => [
                'test' => $element,
            ],
        ]);

        $elements = $factory->createService($container->reveal());
        $this->assertSame($element, $elements->get('test'));
    }

    public function testConfiguresFormElementsServicesWhenFound()
    {
        $element = $this->prophesize(ElementInterface::class)->reveal();
        $config = [
            'form_elements' => [
                'aliases' => [
                    'test' => Number::class,
                ],
                'factories' => [
                    'test-too' => function ($container) use ($element) {
                        return $element;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn($config);

        $factory = new FormElementManagerFactory();
        $elements = $factory($container->reveal(), 'FormElementManager');

        $this->assertInstanceOf(FormElementManager::class, $elements);
        $this->assertTrue($elements->has('test'));
        $this->assertInstanceOf(Number::class, $elements->get('test'));
        $this->assertTrue($elements->has('test-too'));
        $this->assertSame($element, $elements->get('test-too'));
    }

    public function testDoesNotConfigureFormElementsServicesWhenServiceListenerPresent()
    {
        $element = $this->prophesize(ElementInterface::class)->reveal();
        $config = [
            'form_elements' => [
                'aliases' => [
                    'test' => Number::class,
                ],
                'factories' => [
                    'test-too' => function ($container) use ($element) {
                        return $element;
                    },
                ],
            ],
        ];

        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(true);
        $container->has('config')->shouldNotBeCalled();
        $container->get('config')->shouldNotBeCalled();

        $factory = new FormElementManagerFactory();
        $elements = $factory($container->reveal(), 'FormElementManager');

        $this->assertInstanceOf(FormElementManager::class, $elements);
        $this->assertFalse($elements->has('test'));
        $this->assertFalse($elements->has('test-too'));
    }

    public function testDoesNotConfigureFormElementsServicesWhenConfigServiceNotPresent()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(false);
        $container->get('config')->shouldNotBeCalled();

        $factory = new FormElementManagerFactory();
        $elements = $factory($container->reveal(), 'FormElementManager');

        $this->assertInstanceOf(FormElementManager::class, $elements);
    }

    public function testDoesNotConfigureFormElementServicesWhenConfigServiceDoesNotContainFormElementsConfig()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);

        $container->has('ServiceListener')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn(['foo' => 'bar']);
        $container->has('MvcTranslator')->willReturn(false); // necessary due to default initializers

        $factory = new FormElementManagerFactory();
        $elements = $factory($container->reveal(), 'FormElementManager');

        $this->assertInstanceOf(FormElementManager::class, $elements);
        $this->assertFalse($elements->has('foo'));
    }
}

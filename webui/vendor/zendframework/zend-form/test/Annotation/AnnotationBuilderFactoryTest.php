<?php
/**
 * @link      http://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Annotation;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use stdClass;
use Zend\EventManager\EventManagerInterface;
use Zend\EventManager\ListenerAggregateInterface;
use Zend\Form\Annotation\AnnotationBuilder;
use Zend\Form\Annotation\AnnotationBuilderFactory;
use Zend\Form\FormElementManager;
use Zend\ServiceManager\Exception\ServiceNotCreatedException;

class AnnotationBuilderFactoryTest extends TestCase
{
    public function testFactoryReturnsAnnotationBuilder()
    {
        $container = $this->prophesize(ContainerInterface::class);
        $events = $this->prophesize(EventManagerInterface::class);

        $elements = $this->prophesize(FormElementManager::class);
        $container->get('EventManager')->willReturn($events->reveal());
        $container->get('FormElementManager')->willReturn($elements->reveal());
        $container->has('config')->willReturn(false);
        $container->has('InputFilterManager')->willReturn(false);

        $factory = new AnnotationBuilderFactory();
        $this->assertInstanceOf(
            AnnotationBuilder::class,
            $factory($container->reveal(), AnnotationBuilder::class)
        );
    }

    public function testFactoryCanSetPreserveDefinedOrderFlagFromConfiguration()
    {
        $container = $this->prophesize(ContainerInterface::class);
        $events = $this->prophesize(EventManagerInterface::class);

        $elements = $this->prophesize(FormElementManager::class);
        $container->get('EventManager')->willReturn($events->reveal());
        $container->get('FormElementManager')->willReturn($elements->reveal());
        $container->has('InputFilterManager')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'form_annotation_builder' => [
                'preserve_defined_order' => true,
            ],
        ]);

        $factory = new AnnotationBuilderFactory();
        $builder = $factory($container->reveal(), AnnotationBuilder::class);

        $this->assertTrue($builder->preserveDefinedOrder(), 'Preserve defined order was not set correctly');
    }

    public function testFactoryAllowsInjectingAnnotationsFromConfiguration()
    {
        $container = $this->prophesize(ContainerInterface::class);
        $events = $this->prophesize(EventManagerInterface::class);

        $elements = $this->prophesize(FormElementManager::class);
        $container->get('EventManager')->willReturn($events->reveal());
        $container->get('FormElementManager')->willReturn($elements->reveal());
        $container->has('InputFilterManager')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'form_annotation_builder' => [
                'annotations' => [
                    get_class($this),
                ],
            ],
        ]);

        $factory = new AnnotationBuilderFactory();
        $builder = $factory($container->reveal(), AnnotationBuilder::class);

        $parser = $builder->getAnnotationParser();
        $r = new ReflectionProperty($parser, 'allowedAnnotations');
        $r->setAccessible(true);
        $allowedAnnotations = $r->getValue($parser);
        $this->assertContains(get_class($this), $allowedAnnotations);
    }

    public function testFactoryAllowsAttachingListenersFromConfiguration()
    {
        $container = $this->prophesize(ContainerInterface::class);
        $events = $this->prophesize(EventManagerInterface::class);

        $listener = $this->prophesize(ListenerAggregateInterface::class);
        $listener->attach($events->reveal())->shouldBeCalled();

        $elements = $this->prophesize(FormElementManager::class);

        $container->has('InputFilterManager')->willReturn(false);
        $container->get('EventManager')->willReturn($events->reveal());
        $container->get('FormElementManager')->willReturn($elements->reveal());
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'form_annotation_builder' => [
                'listeners' => [
                    'test-listener'
                ],
            ],
        ]);
        $container->get('test-listener')->willReturn($listener->reveal());

        $factory = new AnnotationBuilderFactory();
        $factory($container->reveal(), AnnotationBuilder::class);
    }

    public function testFactoryThrowsExceptionWhenAttachingInvalidListeners()
    {
        $container = $this->prophesize(ContainerInterface::class);
        $events = $this->prophesize(EventManagerInterface::class);
        $listener = $this->prophesize(stdClass::class);

        $elements = $this->prophesize(FormElementManager::class);

        $container->get('EventManager')->willReturn($events->reveal());
        $container->get('FormElementManager')->willReturn($elements->reveal());
        $container->has('InputFilterManager')->willReturn(false);
        $container->has('config')->willReturn(true);
        $container->get('config')->willReturn([
            'form_annotation_builder' => [
                'listeners' => [
                    'test-listener'
                ],
            ],
        ]);
        $container->get('test-listener')->willReturn($listener->reveal());

        $factory = new AnnotationBuilderFactory();

        $this->expectException(ServiceNotCreatedException::class);
        $this->expectExceptionMessage('Invalid event listener');
        $factory($container->reveal(), AnnotationBuilder::class);
    }
}

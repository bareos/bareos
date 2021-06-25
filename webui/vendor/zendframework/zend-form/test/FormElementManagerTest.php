<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form;

use PHPUnit\Framework\TestCase;
use ReflectionProperty;
use Zend\Form\ElementFactory;
use Zend\Form\Exception\InvalidElementException;
use Zend\Form\Factory;
use Zend\Form\Form;
use Zend\Form\FormElementManager;
use Zend\ServiceManager\Exception\InvalidServiceException;
use Zend\ServiceManager\ServiceManager;

/**
 * @group      Zend_Form
 */
class FormElementManagerTest extends TestCase
{
    /**
     * @var FormElementManager
     */
    protected $manager;

    public function setUp()
    {
        $this->manager = new FormElementManager(new ServiceManager());
    }

    public function testInjectToFormFactoryAware()
    {
        $form = $this->manager->get('Form');
        $this->assertSame($this->manager, $form->getFormFactory()->getFormElementManager());
    }

    /**
     * @group 3735
     */
    public function testInjectsFormElementManagerToFormComposedByFormFactoryAwareElement()
    {
        $factory = new Factory();
        $this->manager->setFactory('my-form', function ($elements) use ($factory) {
            $form = new Form();
            $form->setFormFactory($factory);
            return $form;
        });
        $form = $this->manager->get('my-form');
        $this->assertSame($factory, $form->getFormFactory());
        $this->assertSame($this->manager, $form->getFormFactory()->getFormElementManager());
    }

    public function testRegisteringInvalidElementRaisesException()
    {
        $this->expectException($this->getInvalidServiceException());
        $this->manager->setService('test', $this);
    }

    public function testLoadingInvalidElementRaisesException()
    {
        $this->manager->setInvokableClass('test', get_class($this));
        $this->expectException($this->getInvalidServiceException());
        $this->manager->get('test');
    }

    protected function getInvalidServiceException()
    {
        if (method_exists($this->manager, 'configure')) {
            return InvalidServiceException::class;
        }
        return InvalidElementException::class;
    }

    public function testStringCreationOptions()
    {
        $args = 'foo';
        $element = $this->manager->get('element', $args);
        $this->assertEquals('foo', $element->getName(), 'The argument is string');
    }

    public function testArrayCreationOptions()
    {
        $args = [
            'name' => 'foo',
            'options' => [
                'label' => 'bar'
            ],
        ];
        $element = $this->manager->get('element', $args);
        $this->assertEquals('foo', $element->getName(), 'Specified name in array[name]');
        $this->assertEquals('bar', $element->getLabel(), 'Specified options in array[options]');
    }

    public function testOptionsCreationOptions()
    {
        $args = [
            'label' => 'bar'
        ];
        $element = $this->manager->get('element', $args);
        $this->assertEquals('element', $element->getName(), 'Invokable CNAME');
        $this->assertEquals('bar', $element->getLabel(), 'Specified options in array');
    }

    public function testArrayOptionsCreationOptions()
    {
        $args = [
            'options' => [
                'label' => 'bar'
            ],
        ];
        $element = $this->manager->get('element', $args);
        $this->assertEquals('element', $element->getName(), 'Invokable CNAME');
        $this->assertEquals('bar', $element->getLabel(), 'Specified options in array[options]');
    }

    /**
     * @group 6132
     */
    public function testSharedFormElementsAreNotInitializedMultipleTimes()
    {
        $element = $this->getMockBuilder('Zend\Form\Element')
            ->setMethods(['init'])
            ->getMock();

        $element->expects($this->once())->method('init');

        $this->manager->setFactory('sharedElement', function () use ($element) {
            return $element;
        });

        $this->manager->setShared('sharedElement', true);

        $this->manager->get('sharedElement');
        $this->manager->get('sharedElement');
    }

    public function testWillInstantiateFormFromInvokable()
    {
        $form = $this->manager->get('form');
        $this->assertInstanceof(Form::class, $form);
    }

    /**
     * @group 58
     * @group 64
     */
    public function testInjectFactoryInitializerShouldBeRegisteredFirst()
    {
        // @codingStandardsIgnoreStart
        $initializers = [
            function () {},
            function () {},
        ];
        // @codingStandardsIgnoreEnd

        $manager = new FormElementManager(new ServiceManager(), [
            'initializers' => $initializers,
        ]);

        $r = new ReflectionProperty($manager, 'initializers');
        $r->setAccessible(true);
        $actual = $r->getValue($manager);

        $this->assertGreaterThan(2, count($actual));
        $first = array_shift($actual);
        $this->assertSame([$manager, 'injectFactory'], $first);
    }

    /**
     * @group 58
     * @group 64
     */
    public function testCallElementInitInitializerShouldBeRegisteredLast()
    {
        // @codingStandardsIgnoreStart
        $initializers = [
            function () {},
            function () {},
        ];
        // @codingStandardsIgnoreEnd

        $manager = new FormElementManager(new ServiceManager(), [
            'initializers' => $initializers,
        ]);

        $r = new ReflectionProperty($manager, 'initializers');
        $r->setAccessible(true);
        $actual = $r->getValue($manager);

        $this->assertGreaterThan(2, count($actual));
        $last = array_pop($actual);
        $this->assertSame([$manager, 'callElementInit'], $last);
    }

    /**
     * @group 62
     */
    public function testAddingInvokableCreatesAliasAndMapsClassToElementFactory()
    {
        $this->manager->setInvokableClass('foo', TestAsset\ElementWithFilter::class);

        $r = new ReflectionProperty($this->manager, 'aliases');
        $r->setAccessible(true);
        $aliases = $r->getValue($this->manager);

        $this->assertArrayHasKey('foo', $aliases);
        $this->assertEquals(TestAsset\ElementWithFilter::class, $aliases['foo']);

        $r = new ReflectionProperty($this->manager, 'factories');
        $r->setAccessible(true);
        $factories = $r->getValue($this->manager);

        if (method_exists($this->manager, 'configure')) {
            $this->assertArrayHasKey(TestAsset\ElementWithFilter::class, $factories);
            $this->assertEquals(ElementFactory::class, $factories[TestAsset\ElementWithFilter::class]);
        } else {
            $this->assertArrayHasKey('zendtestformtestassetelementwithfilter', $factories);
            $this->assertEquals(ElementFactory::class, $factories['zendtestformtestassetelementwithfilter']);
        }
    }

    public function testAutoAddInvokableClass()
    {
        $instance = $this->manager->get(
            TestAsset\ConstructedElement::class,
            ['constructedKey' => 'constructedKey']
        );
        $this->assertEquals('constructedelement', $instance->getName());
        $this->assertEquals([
            'constructedKey' => 'constructedKey'
        ], $instance->getOptions());
    }

    public function testAllAliasesShouldBeCanonicalized()
    {
        if (method_exists($this->manager, 'configure')) {
            $this->markTestSkipped('Check canonicalized makes sense only on v2');
        }

        $r = new ReflectionProperty($this->manager, 'aliases');
        $r->setAccessible(true);
        $aliases = $r->getValue($this->manager);

        foreach ($aliases as $name => $alias) {
            $this->manager->get($name . ' ');
            $this->manager->get(strtoupper($name));
            $this->manager->get($name);
        }

        $this->addToAssertionCount(1);
    }
}

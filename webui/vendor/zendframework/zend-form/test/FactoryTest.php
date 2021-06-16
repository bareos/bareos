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
use Zend\Filter;
use Zend\Form;
use Zend\Form\Factory as FormFactory;
use Zend\Form\FormElementManager;
use Zend\Hydrator\HydratorPluginManager;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;
use Zend\Hydrator\ObjectProperty;
use Zend\Hydrator\ObjectPropertyHydrator;
use Zend\ServiceManager\ServiceManager;

class FactoryTest extends TestCase
{
    /**
     * @var FormFactory
     */
    protected $factory;

    /**
     * @var ServiceManager
     */
    protected $services;

    public function setUp()
    {
        $this->services = new ServiceManager();
        $elementManager = new FormElementManager($this->services);
        $this->factory = new FormFactory($elementManager);
    }

    public function testCanCreateElements()
    {
        $element = $this->factory->createElement([
            'name'       => 'foo',
            'attributes' => [
                'type'         => 'text',
                'class'        => 'foo-class',
                'data-js-type' => 'my.form.text',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\ElementInterface', $element);
        $this->assertEquals('foo', $element->getName());
        $this->assertEquals('text', $element->getAttribute('type'));
        $this->assertEquals('foo-class', $element->getAttribute('class'));
        $this->assertEquals('my.form.text', $element->getAttribute('data-js-type'));
    }

    public function testCanCreateFieldsets()
    {
        $fieldset = $this->factory->createFieldset([
            'name'       => 'foo',
            'object'     => 'ZendTest\Form\TestAsset\Model',
            'attributes' => [
                'type'         => 'fieldset',
                'class'        => 'foo-class',
                'data-js-type' => 'my.form.fieldset',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $fieldset);
        $this->assertEquals('foo', $fieldset->getName());
        $this->assertEquals('fieldset', $fieldset->getAttribute('type'));
        $this->assertEquals('foo-class', $fieldset->getAttribute('class'));
        $this->assertEquals('my.form.fieldset', $fieldset->getAttribute('data-js-type'));
        $this->assertEquals(new \ZendTest\Form\TestAsset\Model, $fieldset->getObject());
    }

    public function testCanCreateFieldsetsWithElements()
    {
        $fieldset = $this->factory->createFieldset([
            'name'       => 'foo',
            'elements' => [
                [
                    'flags' => [
                        'name' => 'bar',
                    ],
                    'spec' => [
                        'attributes' => [
                            'type' => 'text',
                        ],
                    ],
                ],
                [
                    'flags' => [
                        'name' => 'baz',
                    ],
                    'spec' => [
                        'attributes' => [
                            'type' => 'radio',
                            'options' => [
                                'foo' => 'Foo Bar',
                                'bar' => 'Bar Baz',
                            ],
                        ],
                    ],
                ],
                [
                    'flags' => [
                        'priority' => 10,
                    ],
                    'spec' => [
                        'name'       => 'bat',
                        'attributes' => [
                            'type' => 'textarea',
                            'content' => 'Type here...',
                        ],
                    ],
                ],
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $fieldset);
        $elements = $fieldset->getElements();
        $this->assertEquals(3, count($elements));
        $this->assertTrue($fieldset->has('bar'));
        $this->assertTrue($fieldset->has('baz'));
        $this->assertTrue($fieldset->has('bat'));

        $element = $fieldset->get('bar');
        $this->assertEquals('text', $element->getAttribute('type'));

        $element = $fieldset->get('baz');
        $this->assertEquals('radio', $element->getAttribute('type'));
        $this->assertEquals([
            'foo' => 'Foo Bar',
            'bar' => 'Bar Baz',
        ], $element->getAttribute('options'));

        $element = $fieldset->get('bat');
        $this->assertEquals('textarea', $element->getAttribute('type'));
        $this->assertEquals('Type here...', $element->getAttribute('content'));
        $this->assertEquals('bat', $element->getName());

        // Testing that priority flag is present and works as expected
        foreach ($fieldset as $element) {
            $test = $element->getName();
            break;
        }
        $this->assertEquals('bat', $test);
    }

    public function testCanCreateNestedFieldsets()
    {
        $masterFieldset = $this->factory->createFieldset([
            'name'       => 'foo',
            'fieldsets'  => [
                [
                    'flags' => ['name' => 'bar'],
                    'spec'  => [
                        'elements' => [
                            [
                                'flags' => [
                                    'name' => 'bar',
                                ],
                                'spec' => [
                                    'attributes' => [
                                        'type' => 'text',
                                    ],
                                ],
                            ],
                            [
                                'flags' => [
                                    'name' => 'baz',
                                ],
                                'spec' => [
                                    'attributes' => [
                                        'type' => 'radio',
                                        'options' => [
                                            'foo' => 'Foo Bar',
                                            'bar' => 'Bar Baz',
                                        ],
                                    ],
                                ],
                            ],
                            [
                                'flags' => [
                                    'priority' => 10,
                                ],
                                'spec' => [
                                    'name'       => 'bat',
                                    'attributes' => [
                                        'type' => 'textarea',
                                        'content' => 'Type here...',
                                    ],
                                ],
                            ],
                        ],
                    ]
                ]
            ]
        ]);
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $masterFieldset);
        $fieldsets = $masterFieldset->getFieldsets();
        $this->assertEquals(1, count($fieldsets));
        $this->assertTrue($masterFieldset->has('bar'));

        $fieldset = $masterFieldset->get('bar');
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $fieldset);

        $element = $fieldset->get('bar');
        $this->assertEquals('text', $element->getAttribute('type'));

        $element = $fieldset->get('baz');
        $this->assertEquals('radio', $element->getAttribute('type'));
        $this->assertEquals([
            'foo' => 'Foo Bar',
            'bar' => 'Bar Baz',
        ], $element->getAttribute('options'));

        $element = $fieldset->get('bat');
        $this->assertEquals('textarea', $element->getAttribute('type'));
        $this->assertEquals('Type here...', $element->getAttribute('content'));
        $this->assertEquals('bat', $element->getName());

        // Testing that priority flag is present and works as expected
        foreach ($fieldset as $element) {
            $test = $element->getName();
            break;
        }
        $this->assertEquals('bat', $test);
    }

    public function testCanCreateForms()
    {
        $form = $this->factory->createForm([
            'name'       => 'foo',
            'object'     => 'ZendTest\Form\TestAsset\Model',
            'attributes' => [
                'method' => 'get',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $this->assertEquals('foo', $form->getName());
        $this->assertEquals('get', $form->getAttribute('method'));
        $this->assertEquals(new \ZendTest\Form\TestAsset\Model, $form->getObject());
    }

    public function testCanCreateFormsWithNamedInputFilters()
    {
        $form = $this->factory->createForm([
            'name'         => 'foo',
            'input_filter' => 'ZendTest\Form\TestAsset\InputFilter',
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $filter = $form->getInputFilter();
        $this->assertInstanceOf('ZendTest\Form\TestAsset\InputFilter', $filter);
    }

    public function testCanCreateFormsWithInputFilterSpecifications()
    {
        $form = $this->factory->createForm([
            'name'         => 'foo',
            'input_filter' => [
                'foo' => [
                    'name'       => 'foo',
                    'required'   => false,
                    'validators' => [
                        [
                            'name' => 'NotEmpty',
                        ],
                        [
                            'name' => 'StringLength',
                            'options' => [
                                'min' => 3,
                                'max' => 5,
                            ],
                        ],
                    ],
                ],
                'bar' => [
                    'allow_empty' => true,
                    'filters'     => [
                        [
                            'name' => 'StringTrim',
                        ],
                        [
                            'name' => 'StringToLower',
                            'options' => [
                                'encoding' => 'ISO-8859-1',
                            ],
                        ],
                    ],
                ],
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $filter = $form->getInputFilter();
        $this->assertInstanceOf('Zend\InputFilter\InputFilterInterface', $filter);
        $this->assertEquals(2, count($filter));
        foreach (['foo', 'bar'] as $name) {
            $input = $filter->get($name);

            switch ($name) {
                case 'foo':
                    $this->assertInstanceOf('Zend\InputFilter\Input', $input);
                    $this->assertFalse($input->isRequired());
                    $this->assertEquals(2, count($input->getValidatorChain()));
                    break;
                case 'bar':
                    $this->assertInstanceOf('Zend\InputFilter\Input', $input);
                    $this->assertTrue($input->allowEmpty());
                    $this->assertEquals(2, count($input->getFilterChain()));
                    break;
                default:
                    $this->fail('Unexpected input named "' . $name . '" found in input filter');
            }
        }
    }

    public function testCanCreateFormsWithInputFilterInstances()
    {
        $filter = new TestAsset\InputFilter();
        $form = $this->factory->createForm([
            'name'         => 'foo',
            'input_filter' => $filter,
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $test = $form->getInputFilter();
        $this->assertSame($filter, $test);
    }

    public function testCanCreateFormsAndSpecifyHydrator()
    {
        $hydratorType = class_exists(ObjectPropertyHydrator::class)
            ? ObjectPropertyHydrator::class
            : ObjectProperty::class;

        $form = $this->factory->createForm([
            'name'     => 'foo',
            'hydrator' => $hydratorType,
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($hydratorType, $hydrator);
    }

    public function testCanCreateFormsAndSpecifyHydratorManagedByHydratorManager()
    {
        if (class_exists(ObjectPropertyHydrator::class)) {
            $hydratorShortName = 'ObjectPropertyHydrator';
            $hydratorType      = ObjectPropertyHydrator::class;
        } else {
            $hydratorShortName = 'ObjectProperty';
            $hydratorType      = ObjectProperty::class;
        }

        $this->services->setService('HydratorManager', new HydratorPluginManager($this->services));

        $form = $this->factory->createForm([
            'name'     => 'foo',
            'hydrator' => $hydratorShortName,
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($hydratorType, $hydrator);
    }

    public function testCanCreateHydratorFromArray()
    {
        $hydratorType = class_exists(ClassMethodsHydrator::class)
            ? ClassMethodsHydrator::class
            : ClassMethods::class;

        $form = $this->factory->createForm([
            'name' => 'foo',
            'hydrator' => [
                'type' => $hydratorType,
                'options' => ['underscoreSeparatedKeys' => false],
            ],
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $hydrator = $form->getHydrator();

        $this->assertInstanceOf($hydratorType, $hydrator);
        $this->assertFalse($hydrator->getUnderscoreSeparatedKeys());
    }

    public function testCanCreateHydratorFromConcreteClass()
    {
        $hydratorType = class_exists(ObjectPropertyHydrator::class)
            ? ObjectPropertyHydrator::class
            : ObjectProperty::class;

        $form = $this->factory->createForm([
            'name' => 'foo',
            'hydrator' => new $hydratorType()
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($hydratorType, $hydrator);
    }

    public function testCanCreateFormsAndSpecifyFactory()
    {
        $form = $this->factory->createForm([
            'name'    => 'foo',
            'factory' => 'Zend\Form\Factory',
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $factory = $form->getFormFactory();
        $this->assertInstanceOf('Zend\Form\Factory', $factory);
    }

    public function testCanCreateFactoryFromArray()
    {
        $form = $this->factory->createForm([
            'name'    => 'foo',
            'factory' => [
                'type' => 'Zend\Form\Factory',
            ],
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $factory = $form->getFormFactory();
        $this->assertInstanceOf('Zend\Form\Factory', $factory);
    }

    public function testCanCreateFactoryFromConcreteClass()
    {
        $factory = new \Zend\Form\Factory();
        $form = $this->factory->createForm([
            'name'    => 'foo',
            'factory' => $factory,
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $test = $form->getFormFactory();
        $this->assertSame($factory, $test);
    }

    public function testCanCreateFormFromConcreteClassAndSpecifyCustomValidatorByName()
    {
        $validatorManager = new \Zend\Validator\ValidatorPluginManager($this->services);
        $validatorManager->setInvokableClass('baz', 'Zend\Validator\Digits');

        $defaultValidatorChain = new \Zend\Validator\ValidatorChain();
        $defaultValidatorChain->setPluginManager($validatorManager);

        $inputFilterFactory = new \Zend\InputFilter\Factory();
        $inputFilterFactory->setDefaultValidatorChain($defaultValidatorChain);

        $factory = new FormFactory();
        $factory->setInputFilterFactory($inputFilterFactory);

        $form = $factory->createForm([
            'name'         => 'foo',
            'factory'      => $factory,
            'input_filter' => [
                'bar' => [
                    'name'       => 'bar',
                    'required'   => true,
                    'validators' => [
                        [
                            'name' => 'baz',
                        ],
                    ],
                ],
            ],
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);

        $inputFilter = $form->getInputFilter();
        $this->assertInstanceOf('Zend\InputFilter\InputFilterInterface', $inputFilter);

        $input = $inputFilter->get('bar');
        $this->assertInstanceOf('Zend\InputFilter\Input', $input);

        $validatorChain = $input->getValidatorChain();
        $this->assertInstanceOf('Zend\Validator\ValidatorChain', $validatorChain);

        $validatorArray = $validatorChain->getValidators();
        $found = false;
        foreach ($validatorArray as $validator) {
            $validatorInstance = $validator['instance'];
            $this->assertInstanceOf('Zend\Validator\ValidatorInterface', $validatorInstance);

            if ($validatorInstance instanceof \Zend\Validator\Digits) {
                $found = true;
                break;
            }
        }
        $this->assertTrue($found);
    }

    public function testCanCreateFormFromConcreteClassWithCustomValidatorByNameAndInputFilterFactoryInConstructor()
    {
        $validatorManager = new \Zend\Validator\ValidatorPluginManager($this->services);
        $validatorManager->setInvokableClass('baz', 'Zend\Validator\Digits');

        $defaultValidatorChain = new \Zend\Validator\ValidatorChain();
        $defaultValidatorChain->setPluginManager($validatorManager);

        $inputFilterFactory = new \Zend\InputFilter\Factory();
        $inputFilterFactory->setDefaultValidatorChain($defaultValidatorChain);

        $factory = new FormFactory(null, $inputFilterFactory);

        $form = $factory->createForm([
            'name'         => 'foo',
            'factory'      => $factory,
            'input_filter' => [
                'bar' => [
                    'name'       => 'bar',
                    'required'   => true,
                    'validators' => [
                        [
                            'name' => 'baz',
                        ],
                    ],
                ],
            ],
        ]);

        $this->assertInstanceOf('Zend\Form\FormInterface', $form);

        $inputFilter = $form->getInputFilter();
        $this->assertInstanceOf('Zend\InputFilter\InputFilterInterface', $inputFilter);

        $input = $inputFilter->get('bar');
        $this->assertInstanceOf('Zend\InputFilter\Input', $input);

        $validatorChain = $input->getValidatorChain();
        $this->assertInstanceOf('Zend\Validator\ValidatorChain', $validatorChain);

        $validatorArray = $validatorChain->getValidators();
        $found = false;
        foreach ($validatorArray as $validator) {
            $validatorInstance = $validator['instance'];
            $this->assertInstanceOf('Zend\Validator\ValidatorInterface', $validatorInstance);

            if ($validatorInstance instanceof \Zend\Validator\Digits) {
                $found = true;
                break;
            }
        }
        $this->assertTrue($found);
    }

    public function testCanCreateFormWithHydratorAndInputFilterAndElementsAndFieldsets()
    {
        $hydratorType = class_exists(ObjectPropertyHydrator::class)
            ? ObjectPropertyHydrator::class
            : ObjectProperty::class;

        $form = $this->factory->createForm([
            'name'       => 'foo',
            'elements' => [
                [
                    'flags' => [
                        'name' => 'bar',
                    ],
                    'spec' => [
                        'attributes' => [
                            'type' => 'text',
                        ],
                    ],
                ],
                [
                    'flags' => [
                        'name' => 'baz',
                    ],
                    'spec' => [
                        'attributes' => [
                            'type' => 'radio',
                            'options' => [
                                'foo' => 'Foo Bar',
                                'bar' => 'Bar Baz',
                            ],
                        ],
                    ],
                ],
                [
                    'flags' => [
                        'priority' => 10,
                    ],
                    'spec' => [
                        'name'       => 'bat',
                        'attributes' => [
                            'type' => 'textarea',
                            'content' => 'Type here...',
                        ],
                    ],
                ],
            ],
            'fieldsets'  => [
                [
                    'flags' => ['name' => 'foobar'],
                    'spec'  => [
                        'elements' => [
                            [
                                'flags' => [
                                    'name' => 'bar',
                                ],
                                'spec' => [
                                    'attributes' => [
                                        'type' => 'text',
                                    ],
                                ],
                            ],
                            [
                                'flags' => [
                                    'name' => 'baz',
                                ],
                                'spec' => [
                                    'attributes' => [
                                        'type' => 'radio',
                                        'options' => [
                                            'foo' => 'Foo Bar',
                                            'bar' => 'Bar Baz',
                                        ],
                                    ],
                                ],
                            ],
                            [
                                'flags' => [
                                    'priority' => 10,
                                ],
                                'spec' => [
                                    'name'       => 'bat',
                                    'attributes' => [
                                        'type' => 'textarea',
                                        'content' => 'Type here...',
                                    ],
                                ],
                            ],
                        ],
                    ],
                ],
            ],
            'input_filter' => 'ZendTest\Form\TestAsset\InputFilter',
            'hydrator'     => $hydratorType,
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);

        $elements = $form->getElements();
        $this->assertEquals(3, count($elements));
        $this->assertTrue($form->has('bar'));
        $this->assertTrue($form->has('baz'));
        $this->assertTrue($form->has('bat'));

        $element = $form->get('bar');
        $this->assertEquals('text', $element->getAttribute('type'));

        $element = $form->get('baz');
        $this->assertEquals('radio', $element->getAttribute('type'));
        $this->assertEquals([
            'foo' => 'Foo Bar',
            'bar' => 'Bar Baz',
        ], $element->getAttribute('options'));

        $element = $form->get('bat');
        $this->assertEquals('textarea', $element->getAttribute('type'));
        $this->assertEquals('Type here...', $element->getAttribute('content'));
        $this->assertEquals('bat', $element->getName());

        // Testing that priority flag is present and works as expected
        foreach ($form as $element) {
            $test = $element->getName();
            break;
        }
        $this->assertEquals('bat', $test);

        // Test against nested fieldset
        $fieldsets = $form->getFieldsets();
        $this->assertEquals(1, count($fieldsets));
        $this->assertTrue($form->has('foobar'));

        $fieldset = $form->get('foobar');
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $fieldset);

        $element = $fieldset->get('bar');
        $this->assertEquals('text', $element->getAttribute('type'));

        $element = $fieldset->get('baz');
        $this->assertEquals('radio', $element->getAttribute('type'));
        $this->assertEquals([
            'foo' => 'Foo Bar',
            'bar' => 'Bar Baz',
        ], $element->getAttribute('options'));

        $element = $fieldset->get('bat');
        $this->assertEquals('textarea', $element->getAttribute('type'));
        $this->assertEquals('Type here...', $element->getAttribute('content'));
        $this->assertEquals('bat', $element->getName());

        // Testing that priority flag is present and works as expected
        foreach ($fieldset as $element) {
            $test = $element->getName();
            break;
        }
        $this->assertEquals('bat', $test);

        // input filter
        $filter = $form->getInputFilter();
        $this->assertInstanceOf('ZendTest\Form\TestAsset\InputFilter', $filter);

        // hydrator
        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($hydratorType, $hydrator);
    }

    public function testCanCreateFormUsingCreate()
    {
        $form = $this->factory->create([
            'type'       => 'Zend\Form\Form',
            'name'       => 'foo',
            'attributes' => [
                'method' => 'get',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);
        $this->assertEquals('foo', $form->getName());
        $this->assertEquals('get', $form->getAttribute('method'));
    }

    public function testCanCreateFieldsetUsingCreate()
    {
        $fieldset = $this->factory->create([
            'type'       => 'Zend\Form\Fieldset',
            'name'       => 'foo',
            'attributes' => [
                'type'         => 'fieldset',
                'class'        => 'foo-class',
                'data-js-type' => 'my.form.fieldset',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FieldsetInterface', $fieldset);
        $this->assertEquals('foo', $fieldset->getName());
        $this->assertEquals('fieldset', $fieldset->getAttribute('type'));
        $this->assertEquals('foo-class', $fieldset->getAttribute('class'));
        $this->assertEquals('my.form.fieldset', $fieldset->getAttribute('data-js-type'));
    }

    public function testCanCreateElementUsingCreate()
    {
        $element = $this->factory->create([
            'name'       => 'foo',
            'attributes' => [
                'type'         => 'text',
                'class'        => 'foo-class',
                'data-js-type' => 'my.form.text',
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\ElementInterface', $element);
        $this->assertEquals('foo', $element->getName());
        $this->assertEquals('text', $element->getAttribute('type'));
        $this->assertEquals('foo-class', $element->getAttribute('class'));
        $this->assertEquals('my.form.text', $element->getAttribute('data-js-type'));
    }

    public function testAutomaticallyAddFieldsetTypeWhenCreateFieldset()
    {
        $fieldset = $this->factory->createFieldset(['name' => 'myFieldset']);
        $this->assertInstanceOf('Zend\Form\Fieldset', $fieldset);
        $this->assertEquals('myFieldset', $fieldset->getName());
    }

    public function testAutomaticallyAddFormTypeWhenCreateForm()
    {
        $form = $this->factory->createForm(['name' => 'myForm']);
        $this->assertInstanceOf('Zend\Form\Form', $form);
        $this->assertEquals('myForm', $form->getName());
    }

    public function testCanPullHydratorThroughServiceManager()
    {
        $hydratorType = class_exists(ObjectPropertyHydrator::class)
            ? ObjectPropertyHydrator::class
            : ObjectProperty::class;

        $this->services->setInvokableClass('MyHydrator', $hydratorType);

        $fieldset = $this->factory->createFieldset([
            'hydrator' => 'MyHydrator',
            'name' => 'fieldset',
            'elements' => [
                [
                    'flags' => [
                        'name' => 'bar',
                    ],
                ]
            ]
        ]);

        $this->assertInstanceOf($hydratorType, $fieldset->getHydrator());
    }

    public function testCreatedFieldsetsHaveFactoryAndFormElementManagerInjected()
    {
        $fieldset = $this->factory->createFieldset(['name' => 'myFieldset']);
        $this->assertAttributeInstanceOf('Zend\Form\Factory', 'factory', $fieldset);
        $this->assertSame(
            $fieldset->getFormFactory()->getFormElementManager(),
            $this->factory->getFormElementManager()
        );
    }

    /**
     * @group 6949
     */
    public function testPrepareAndInjectWillThrowAndException()
    {
        $fieldset = $this->factory->createFieldset(['name' => 'myFieldset']);

        $this->expectException('Zend\Form\Exception\DomainException');
        $this->factory->configureFieldset($fieldset, ['hydrator' => 0]);
    }

    public function testCanCreateFormWithNullElements()
    {
        $form = $this->factory->createForm([
            'name' => 'foo',
            'elements' => [
                'bar' => [
                    'spec' => [
                        'name' => 'bar',
                    ],
                ],
                'baz' => null,
                'bat' => [
                    'spec' => [
                        'name' => 'bat',
                    ],
                ],
            ],
        ]);
        $this->assertInstanceOf('Zend\Form\FormInterface', $form);

        $elements = $form->getElements();
        $this->assertEquals(2, count($elements));
        $this->assertTrue($form->has('bar'));
        $this->assertFalse($form->has('baz'));
        $this->assertTrue($form->has('bat'));
    }

    public function testCanCreateWithConstructionLogicInOptions()
    {
        $formManager = $this->factory->getFormElementManager();
        $formManager->setFactory(
            TestAsset\FieldsetWithDependency::class,
            TestAsset\FieldsetWithDependencyFactory::class
        );

        $collection = $this->factory->create([
            'type' => Form\Element\Collection::class,
            'name' => 'my_fieldset_collection',
            'options' => [
                'target_element' => [
                    'type' => TestAsset\FieldsetWithDependency::class,
                ],
            ],
        ]);

        $this->assertInstanceOf(Form\Element\Collection::class, $collection);

        $targetElement = $collection->getTargetElement();

        $this->assertInstanceOf(TestAsset\FieldsetWithDependency::class, $targetElement);
        $this->assertInstanceOf(TestAsset\InputFilter::class, $targetElement->getDependency());
    }
}

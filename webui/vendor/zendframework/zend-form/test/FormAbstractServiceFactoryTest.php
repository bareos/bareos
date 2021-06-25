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
use Zend\Filter\FilterPluginManager;
use Zend\Form\FormAbstractServiceFactory;
use Zend\Form\FormElementManager;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\ServiceManager\ServiceManager;
use Zend\Hydrator\HydratorPluginManager;
use Zend\Hydrator\ObjectProperty;
use Zend\Hydrator\ObjectPropertyHydrator;
use Zend\Validator\ValidatorPluginManager;

class FormAbstractServiceFactoryTest extends TestCase
{
    public function setUp()
    {
        $this->objectPropertyHydratorClass = class_exists(ObjectPropertyHydrator::class)
            ? ObjectPropertyHydrator::class
            : ObjectProperty::class;

        $services     = $this->services = new ServiceManager;
        $elements     = new FormElementManager($services);
        $filters      = new FilterPluginManager($services);
        $hydrators    = new HydratorPluginManager($services);
        $inputFilters = new InputFilterPluginManager($services);
        $validators   = new ValidatorPluginManager($services);

        $services->setService('FilterManager', $filters);
        $services->setService('FormElementManager', $elements);
        $services->setService('HydratorManager', $hydrators);
        $services->setService('InputFilterManager', $inputFilters);
        $services->setService('ValidatorManager', $validators);

        $inputFilters->setInvokableClass('FooInputFilter', 'Zend\InputFilter\InputFilter');

        $forms = $this->forms = new FormAbstractServiceFactory($services);
        $services->addAbstractFactory($forms);
    }

    public function testMissingConfigServiceIndicatesCannotCreateForm()
    {
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'foo', 'foo'));
    }

    public function testMissingFormServicePrefixIndicatesCannotCreateForm()
    {
        $this->services->setService('config', []);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'foo', 'foo'));
    }

    public function testMissingFormManagerConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', []);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Form\Foo', 'Form\Foo'));
    }

    public function testInvalidFormManagerConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', ['forms' => 'string']);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Form\Foo', 'Form\Foo'));
    }

    public function testEmptyFormManagerConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', ['forms' => []]);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Form\Foo', 'Form\Foo'));
    }

    public function testMissingFormConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', [
            'forms' => [
                'Bar' => [],
            ],
        ]);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Form\Foo', 'Form\Foo'));
    }

    public function testInvalidFormConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', [
            'forms' => [
                'Foo' => 'string',
            ],
        ]);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Foo', 'Foo'));
    }

    public function testEmptyFormConfigIndicatesCannotCreateForm()
    {
        $this->services->setService('config', [
            'forms' => [
                'Foo' => [],
            ],
        ]);
        $this->assertFalse($this->forms->canCreateServiceWithName($this->services, 'Foo', 'Foo'));
    }

    public function testPopulatedFormConfigIndicatesFormCanBeCreated()
    {
        $this->services->setService('config', [
            'forms' => [
                'Foo' => [
                    'type'     => 'Zend\Form\Form',
                    'elements' => [],
                ],
            ],
        ]);
        $this->assertTrue($this->forms->canCreateServiceWithName($this->services, 'Foo', 'Foo'));
    }

    public function testFormCanBeCreatedViaInteractionOfAllManagers()
    {
        $formConfig = [
            'hydrator' => class_exists(ObjectPropertyHydrator::class)
                ? 'ObjectPropertyHydrator'
                : 'ObjectProperty',
            'type'     => 'Zend\Form\Form',
            'elements' => [
                [
                    'spec' => [
                        'type' => 'Zend\Form\Element\Email',
                        'name' => 'email',
                        'options' => [
                            'label' => 'Your email address',
                        ]
                    ],
                ],
            ],
            'input_filter' => 'FooInputFilter',
        ];
        $config = ['forms' => ['Foo' => $formConfig]];
        $this->services->setService('config', $config);
        $form = $this->forms->createServiceWithName($this->services, 'Foo', 'Foo');
        $this->assertInstanceOf('Zend\Form\Form', $form);

        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($this->objectPropertyHydratorClass, $hydrator);

        $inputFilter = $form->getInputFilter();
        $this->assertInstanceOf('Zend\InputFilter\InputFilter', $inputFilter);

        $inputFactory = $inputFilter->getFactory();
        $this->assertInstanceOf('Zend\InputFilter\Factory', $inputFactory);
        $filters      = $this->services->get('FilterManager');
        $validators   = $this->services->get('ValidatorManager');
        $this->assertSame($filters, $inputFactory->getDefaultFilterChain()->getPluginManager());
        $this->assertSame($validators, $inputFactory->getDefaultValidatorChain()->getPluginManager());
    }

    public function testFormCanBeCreatedViaInteractionOfAllManagersExceptInputFilterManager()
    {
        $formConfig = [
            'hydrator' => class_exists(ObjectPropertyHydrator::class)
                ? 'ObjectPropertyHydrator'
                : 'ObjectProperty',
            'type'     => 'Zend\Form\Form',
            'elements' => [
                [
                    'spec' => [
                        'type' => 'Zend\Form\Element\Email',
                        'name' => 'email',
                        'options' => [
                            'label' => 'Your email address',
                        ]
                    ],
                ],
            ],
            'input_filter' => [
                'email' => [
                    'required'   => true,
                    'filters'    => [
                        [
                            'name' => 'StringTrim',
                        ],
                    ],
                    'validators' => [
                        [
                            'name' => 'EmailAddress',
                        ],
                    ],
                ],
            ],
        ];
        $config = ['forms' => ['Foo' => $formConfig]];
        $this->services->setService('config', $config);
        $form = $this->forms->createServiceWithName($this->services, 'Foo', 'Foo');
        $this->assertInstanceOf('Zend\Form\Form', $form);

        $hydrator = $form->getHydrator();
        $this->assertInstanceOf($this->objectPropertyHydratorClass, $hydrator);

        $inputFilter = $form->getInputFilter();
        $this->assertInstanceOf('Zend\InputFilter\InputFilter', $inputFilter);

        $inputFactory = $inputFilter->getFactory();
        $filters      = $this->services->get('FilterManager');
        $validators   = $this->services->get('ValidatorManager');
        $this->assertSame($filters, $inputFactory->getDefaultFilterChain()->getPluginManager());
        $this->assertSame($validators, $inputFactory->getDefaultValidatorChain()->getPluginManager());
    }
}

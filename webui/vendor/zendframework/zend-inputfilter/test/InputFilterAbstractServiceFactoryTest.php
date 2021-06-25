<?php

/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use PHPUnit_Framework_MockObject_MockObject as MockObject;
use PHPUnit\Framework\TestCase;
use Zend\Filter;
use Zend\Filter\FilterPluginManager;
use Zend\InputFilter\FileInput;
use Zend\InputFilter\InputFilterAbstractServiceFactory;
use Zend\InputFilter\InputFilterInterface;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\ServiceManager\ServiceManager;
use Zend\Validator;
use Zend\Validator\ValidatorInterface;
use Zend\Validator\ValidatorPluginManager;

/**
 * @covers \Zend\InputFilter\InputFilterAbstractServiceFactory
 */
class InputFilterAbstractServiceFactoryTest extends TestCase
{
    /**
     * @var ServiceManager
     */
    protected $services;

    /**
     * @var InputFilterPluginManager
    */
    protected $filters;

    /**
     * @var InputFilterAbstractServiceFactory
     */
    protected $factory;

    protected function setUp()
    {
        $this->services = new ServiceManager();
        $this->filters  = new InputFilterPluginManager($this->services);
        $this->services->setService('InputFilterManager', $this->filters);

        $this->factory = new InputFilterAbstractServiceFactory();
    }

    public function testCannotCreateServiceIfNoConfigServicePresent()
    {
        if (method_exists($this->services, 'configure')) {
            // v3
            $method = 'canCreate';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'canCreateServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $this->assertFalse(call_user_func_array([$this->factory, $method], $args));
    }

    public function testCannotCreateServiceIfConfigServiceDoesNotHaveInputFiltersConfiguration()
    {
        $this->services->setService('config', []);
        if (method_exists($this->services, 'configure')) {
            // v3
            $method = 'canCreate';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'canCreateServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $this->assertFalse(call_user_func_array([$this->factory, $method], $args));
    }

    public function testCannotCreateServiceIfConfigInputFiltersDoesNotContainMatchingServiceName()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [],
        ]);
        if (method_exists($this->services, 'configure')) {
            // v3
            $method = 'canCreate';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'canCreateServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $this->assertFalse(call_user_func_array([$this->factory, $method], $args));
    }

    public function testCanCreateServiceIfConfigInputFiltersContainsMatchingServiceName()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [
                'filter' => [],
            ],
        ]);
        if (method_exists($this->services, 'configure')) {
            // v3
            $method = 'canCreate';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'canCreateServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $this->assertTrue(call_user_func_array([$this->factory, $method], $args));
    }

    public function testCreatesInputFilterInstance()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [
                'filter' => [],
            ],
        ]);
        if (method_exists($this->services, 'configure')) {
            // v3
            $method = '__invoke';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'createServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $filter = call_user_func_array([$this->factory, $method], $args);
        $this->assertInstanceOf(InputFilterInterface::class, $filter);
    }

    /**
     * @depends testCreatesInputFilterInstance
     */
    public function testUsesConfiguredValidationAndFilterManagerServicesWhenCreatingInputFilter()
    {
        $filters = new FilterPluginManager($this->services);
        $filter  = function ($value) {
        };
        $filters->setService('foo', $filter);

        $validators = new ValidatorPluginManager($this->services);
        /** @var ValidatorInterface|MockObject $validator */
        $validator  = $this->createMock(ValidatorInterface::class);
        $validators->setService('foo', $validator);

        $this->services->setService('FilterManager', $filters);
        $this->services->setService('ValidatorManager', $validators);
        $this->services->setService('config', [
            'input_filter_specs' => [
                'filter' => [
                    'input' => [
                        'name' => 'input',
                        'required' => true,
                        'filters' => [
                            [ 'name' => 'foo' ],
                        ],
                        'validators' => [
                            [ 'name' => 'foo' ],
                        ],
                    ],
                ],
            ],
        ]);


        if (method_exists($this->services, 'configure')) {
            // v3
            $method = '__invoke';
            $args = [$this->getCompatContainer(), 'filter'];
        } else {
            // v2
            $method = 'createServiceWithName';
            $args = [$this->getCompatContainer(), 'filter', 'filter'];
        }
        $inputFilter = call_user_func_array([$this->factory, $method], $args);
        $this->assertTrue($inputFilter->has('input'));

        $input = $inputFilter->get('input');

        $filterChain = $input->getFilterChain();
        $this->assertSame($filters, $filterChain->getPluginManager());
        $this->assertEquals(1, count($filterChain));
        $this->assertSame($filter, $filterChain->plugin('foo'));
        $this->assertEquals(1, count($filterChain));

        $validatorChain = $input->getValidatorChain();
        $this->assertSame($validators, $validatorChain->getPluginManager());
        $this->assertEquals(1, count($validatorChain));
        $this->assertSame($validator, $validatorChain->plugin('foo'));
        $this->assertEquals(1, count($validatorChain));
    }

    public function testRetrieveInputFilterFromInputFilterPluginManager()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [
                'foobar' => [
                    'input' => [
                        'name' => 'input',
                        'required' => true,
                        'filters' => [
                            [ 'name' => 'foo' ],
                        ],
                        'validators' => [
                            [ 'name' => 'foo' ],
                        ],
                    ],
                ],
            ],
        ]);
        $validators = new ValidatorPluginManager($this->services);
        /** @var ValidatorInterface|MockObject $validator */
        $validator  = $this->createMock(ValidatorInterface::class);
        $this->services->setService('ValidatorManager', $validators);
        $validators->setService('foo', $validator);

        $filters = new FilterPluginManager($this->services);
        $filter  = function ($value) {
        };
        $filters->setService('foo', $filter);

        $this->services->setService('FilterManager', $filters);
        $this->services->get('InputFilterManager')->addAbstractFactory(InputFilterAbstractServiceFactory::class);

        $inputFilter = $this->services->get('InputFilterManager')->get('foobar');
        $this->assertInstanceOf(InputFilterInterface::class, $inputFilter);
    }

    /**
     * Returns appropriate instance to pass to `canCreate()` et al depending on SM version
     *
     * v3 passes the 'creationContext' (ie the root SM) to the AbstractFactory, whereas v2 passes the PluginManager
     */
    protected function getCompatContainer()
    {
        if (method_exists($this->services, 'configure')) {
            // v3
            return $this->services;
        } else {
            // v2
            return $this->filters;
        }
    }


    /**
     * @depends testCreatesInputFilterInstance
     */
    public function testInjectsInputFilterManagerFromServiceManager()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [
                'filter' => [],
            ],
        ]);
        $this->filters->addAbstractFactory(TestAsset\FooAbstractFactory::class);

        if (method_exists($this->filters, 'configure')) {
            // zend-servicemanager v3 usage
            $filter = $this->factory->__invoke($this->services, 'filter');
        } else {
            // zend-servicemanager v2 usage
            $filter = $this->factory->createServiceWithName($this->filters, 'filter', 'filter');
        }

        $inputFilterManager = $filter->getFactory()->getInputFilterManager();

        $this->assertInstanceOf('Zend\InputFilter\InputFilterPluginManager', $inputFilterManager);
        $this->assertInstanceOf('ZendTest\InputFilter\TestAsset\Foo', $inputFilterManager->get('foo'));
    }

    /**
     * @group zendframework/zend-servicemanager#123
     */
    public function testAllowsPassingNonPluginManagerContainerToFactoryWithServiceManagerV2()
    {
        $this->services->setService('config', [
            'input_filter_specs' => [
                'filter' => [],
            ],
        ]);
        if (method_exists($this->services, 'configure')) {
            // v3
            $canCreate = 'canCreate';
            $create = '__invoke';
            $args = [$this->services, 'filter'];
        } else {
            // v2
            $canCreate = 'canCreateServiceWithName';
            $create = 'createServiceWithName';
            $args = [$this->services, 'filter', 'filter'];
        }

        $this->assertTrue(call_user_func_array([$this->factory, $canCreate], $args));
        $inputFilter = call_user_func_array([$this->factory, $create], $args);
        $this->assertInstanceOf(InputFilterInterface::class, $inputFilter);
    }

    /**
     * @see https://github.com/zendframework/zend-inputfilter/issues/155
     */
    public function testWillUseCustomFiltersWhenProvided()
    {
        $filter = $this->prophesize(Filter\FilterInterface::class)->reveal();

        $filters = new FilterPluginManager($this->services);
        $filters->setService('CustomFilter', $filter);

        $validators = new ValidatorPluginManager($this->services);

        $this->services->setService('FilterManager', $filters);
        $this->services->setService('ValidatorManager', $validators);

        $this->services->setService('config', [
            'input_filter_specs' => [
                'test' => [
                    [
                        'name' => 'a-file-element',
                        'type' => FileInput::class,
                        'required' => true,
                        'validators' => [
                            [
                                'name' => Validator\File\UploadFile::class,
                                'options' => [
                                    'breakchainonfailure' => true,
                                ],
                            ],
                            [
                                'name' => Validator\File\Size::class,
                                'options' => [
                                    'breakchainonfailure' => true,
                                    'max' => '6GB',
                                ],
                            ],
                            [
                                'name' => Validator\File\Extension::class,
                                'options' => [
                                    'breakchainonfailure' => true,
                                    'extension' => 'csv,zip',
                                ],
                            ],
                        ],
                        'filters' => [
                            ['name' => 'CustomFilter'],
                        ],
                    ],
                ],
            ],
        ]);

        $this->services->get('InputFilterManager')
            ->addAbstractFactory(InputFilterAbstractServiceFactory::class);

        $inputFilter = $this->services->get('InputFilterManager')->get('test');
        $this->assertInstanceOf(InputFilterInterface::class, $inputFilter);

        $input = $inputFilter->get('a-file-element');
        $this->assertInstanceOf(FileInput::class, $input);

        $filters = $input->getFilterChain();
        $this->assertCount(1, $filters);

        $callback = $filters->getFilters()->top();
        $this->assertInternalType('array', $callback);
        $this->assertSame($filter, $callback[0]);
    }
}

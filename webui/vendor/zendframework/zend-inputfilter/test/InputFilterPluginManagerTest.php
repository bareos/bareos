<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use PHPUnit\Framework\TestCase;
use PHPUnit_Framework_MockObject_MockObject as MockObject;
use Zend\Filter\FilterPluginManager;
use Zend\InputFilter\CollectionInputFilter;
use Zend\InputFilter\Exception\RuntimeException;
use Zend\InputFilter\InputFilter;
use Zend\InputFilter\InputFilterInterface;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\InputFilter\InputInterface;
use Zend\ServiceManager\AbstractPluginManager;
use Zend\ServiceManager\Exception\InvalidServiceException;
use Zend\ServiceManager\ServiceLocatorInterface;
use Zend\ServiceManager\ServiceManager;
use Zend\Stdlib\InitializableInterface;
use Zend\Validator\ValidatorPluginManager;

/**
 * @covers \Zend\InputFilter\InputFilterPluginManager
 */
class InputFilterPluginManagerTest extends TestCase
{
    /**
     * @var InputFilterPluginManager
     */
    protected $manager;

    /**
     * @var ServiceManager
     */
    protected $services;

    protected function setUp()
    {
        $this->services = new ServiceManager();
        $this->manager = new InputFilterPluginManager($this->services);
    }

    public function testIsASubclassOfAbstractPluginManager()
    {
        $this->assertInstanceOf(AbstractPluginManager::class, $this->manager);
    }

    public function testIsNotSharedByDefault()
    {
        $property = method_exists($this->manager, 'configure')
            ? 'sharedByDefault' // v3
            : 'shareByDefault'; // v2

        $this->assertAttributeSame(false, $property, $this->manager);
    }

    public function testRegisteringInvalidElementRaisesException()
    {
        $this->expectException($this->getServiceNotFoundException());
        $this->expectExceptionMessage(
            'must implement Zend\InputFilter\InputFilterInterface or Zend\InputFilter\InputInterface'
        );
        $this->manager->setService('test', $this);
    }

    public function testLoadingInvalidElementRaisesException()
    {
        $this->manager->setInvokableClass('test', get_class($this));
        $this->expectException($this->getServiceNotFoundException());
        $this->manager->get('test');
    }

    public function defaultInvokableClassesProvider()
    {
        return [
            // Description => [$alias, $expectedInstance]
            'inputfilter' => ['inputfilter', InputFilter::class],
            'collection' => ['collection', CollectionInputFilter::class],
        ];
    }

    /**
     * @dataProvider defaultInvokableClassesProvider
     */
    public function testDefaultInvokableClasses($alias, $expectedInstance)
    {
        $service = $this->manager->get($alias);

        $this->assertInstanceOf($expectedInstance, $service, 'get() return type not match');
    }

    public function testInputFilterInvokableClassSMDependenciesArePopulatedWithoutServiceLocator()
    {
        /** @var InputFilter $service */
        $service = $this->manager->get('inputfilter');

        $factory = $service->getFactory();
        $this->assertSame(
            $this->manager,
            $factory->getInputFilterManager(),
            'Factory::getInputFilterManager() is not populated with the expected plugin manager'
        );
    }

    public function testInputFilterInvokableClassSMDependenciesArePopulatedWithServiceLocator()
    {
        $filterManager = $this->getMockBuilder(FilterPluginManager::class)
            ->disableOriginalConstructor()
            ->getMock();
        $validatorManager = $this->getMockBuilder(ValidatorPluginManager::class)
            ->disableOriginalConstructor()
            ->getMock();

        $this->services->setService('FilterManager', $filterManager);
        $this->services->setService('ValidatorManager', $validatorManager);

        /** @var InputFilter $service */
        $service = $this->manager->get('inputfilter');

        $factory = $service->getFactory();
        $this->assertSame(
            $this->manager,
            $factory->getInputFilterManager(),
            'Factory::getInputFilterManager() is not populated with the expected plugin manager'
        );

        $defaultFilterChain = $factory->getDefaultFilterChain();
        $this->assertSame(
            $filterManager,
            $defaultFilterChain->getPluginManager(),
            'Factory::getDefaultFilterChain() is not populated with the expected plugin manager'
        );

        $defaultValidatorChain = $factory->getDefaultValidatorChain();
        $this->assertSame(
            $validatorManager,
            $defaultValidatorChain->getPluginManager(),
            'Factory::getDefaultValidatorChain() is not populated with the expected plugin manager'
        );
    }

    public function serviceProvider()
    {
        $inputFilterInterfaceMock = $this->createInputFilterInterfaceMock();
        $inputInterfaceMock = $this->createInputInterfaceMock();

        // @codingStandardsIgnoreStart
        return [
            // Description         => [$serviceName,                  $service,                  $instanceOf]
            'InputFilterInterface' => ['inputFilterInterfaceService', $inputFilterInterfaceMock, InputFilterInterface::class],
            'InputInterface'       => ['inputInterfaceService',       $inputInterfaceMock,       InputInterface::class],
        ];
        // @codingStandardsIgnoreEnd
    }

    /**
     * @dataProvider serviceProvider
     */
    public function testGet($serviceName, $service)
    {
        $this->manager->setService($serviceName, $service);

        $this->assertSame($service, $this->manager->get($serviceName), 'get() value not match');
    }

    /**
     * @dataProvider serviceProvider
     */
    public function testServicesAreInitiatedIfImplementsInitializableInterface($serviceName, $service, $instanceOf)
    {
        $initializableProphecy = $this->prophesize($instanceOf)->willImplement(InitializableInterface::class);
        $service = $initializableProphecy->reveal();

        $this->manager->setService($serviceName, $service);
        $this->assertSame($service, $this->manager->get($serviceName), 'get() value not match');

        /** @noinspection PhpUndefinedMethodInspection */
        $initializableProphecy->init()->shouldBeCalled();
    }

    public function testPopulateFactoryCanAcceptInputFilterAsFirstArgumentAndWillUseFactoryWhenItDoes()
    {
        $inputFilter = new InputFilter();
        $this->manager->populateFactory($inputFilter);

        $this->assertSame($this->manager, $inputFilter->getFactory()->getInputFilterManager());
    }

    /**
     * @return MockObject|InputFilterInterface
     */
    protected function createInputFilterInterfaceMock()
    {
        /** @var InputFilterInterface|MockObject $inputFilter */
        $inputFilter = $this->createMock(InputFilterInterface::class);

        return $inputFilter;
    }

    /**
     * @return MockObject|InputInterface
     */
    protected function createInputInterfaceMock()
    {
        /** @var InputInterface|MockObject $input */
        $input = $this->createMock(InputInterface::class);

        return $input;
    }

    /**
     * @return MockObject|ServiceLocatorInterface
     */
    protected function createServiceLocatorInterfaceMock()
    {
        /** @var ServiceLocatorInterface|MockObject $serviceLocator */
        $serviceLocator = $this->createMock(ServiceLocatorInterface::class);

        return $serviceLocator;
    }

    protected function getServiceNotFoundException()
    {
        if (method_exists($this->manager, 'configure')) {
            return InvalidServiceException::class;
        }
        return RuntimeException::class;
    }
}

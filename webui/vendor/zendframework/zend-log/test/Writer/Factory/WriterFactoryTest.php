<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Writer\Factory;

use Interop\Container\ContainerInterface;
use PHPUnit\Framework\TestCase;
use Zend\Log\Writer\Factory\WriterFactory;
use Zend\ServiceManager\AbstractPluginManager;
use Zend\ServiceManager\Exception\InvalidServiceException;
use Zend\ServiceManager\ServiceLocatorInterface;
use ZendTest\Log\Writer\TestAsset\InvokableObject;

class WriterFactoryTest extends TestCase
{
    protected function createServiceManagerMock()
    {
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);
        return $container;
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::setCreationOptions
     */
    public function testSetCreationOptions()
    {
        // Arrange
        $container = $this->prophesize(ServiceLocatorInterface::class);
        $container->willImplement(ContainerInterface::class);
        $container = $container->reveal();

        $pluginManager = $this->prophesize(AbstractPluginManager::class);
        $pluginManager->getServiceLocator()->willReturn($container);
        $pluginManager = $pluginManager->reveal();

        $factory = new WriterFactory();
        $factory->setCreationOptions(['foo' => 'bar']);

        // Act
        $object = $factory->createService($pluginManager, InvokableObject::class, InvokableObject::class);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals(['foo' => 'bar'], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__construct
     * @covers \Zend\Log\Writer\Factory\WriterFactory::createService
     */
    public function testCreateServiceWithoutCreationOptions()
    {
        // Arrange
        $container = $this->createServiceManagerMock();

        $pluginManager = $this->prophesize(AbstractPluginManager::class);
        $pluginManager->getServiceLocator()->willReturn($container->reveal());
        $pluginManager = $pluginManager->reveal();

        $factory = new WriterFactory();

        // Act
        $object = $factory->createService($pluginManager, InvokableObject::class, InvokableObject::class);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__construct
     * @covers \Zend\Log\Writer\Factory\WriterFactory::createService
     */
    public function testCreateServiceWithCreationOptions()
    {
        // Arrange
        $container = $this->createServiceManagerMock();

        $pluginManager = $this->prophesize(AbstractPluginManager::class);
        $pluginManager->getServiceLocator()->willReturn($container->reveal());
        $pluginManager = $pluginManager->reveal();

        $factory = new WriterFactory(['foo' => 'bar']);

        // Act
        $object = $factory->createService($pluginManager, InvokableObject::class, InvokableObject::class);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals(['foo' => 'bar'], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__construct
     * @covers \Zend\Log\Writer\Factory\WriterFactory::createService
     */
    public function testCreateServiceWithValidRequestName()
    {
        // Arrange
        $container = $this->createServiceManagerMock();

        $pluginManager = $this->prophesize(AbstractPluginManager::class);
        $pluginManager->getServiceLocator()->willReturn($container->reveal());
        $pluginManager = $pluginManager->reveal();

        $factory = new WriterFactory();

        // Act
        $object = $factory->createService($pluginManager, 'invalid', InvokableObject::class);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__construct
     * @covers \Zend\Log\Writer\Factory\WriterFactory::createService
     */
    public function testCreateServiceInvalidNames()
    {
        // Arrange
        $container = $this->createServiceManagerMock();

        $pluginManager = $this->prophesize(AbstractPluginManager::class);
        $pluginManager->getServiceLocator()->willReturn($container->reveal());
        $pluginManager = $pluginManager->reveal();

        $factory = new WriterFactory();

        // Assert
        $this->expectException(InvalidServiceException::class);
        $this->expectExceptionMessage('WriterFactory requires that the requested name is provided');

        // Act
        $object = $factory->createService($pluginManager, 'invalid', 'invalid');
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithoutOptions()
    {
        // Arrange
        $container = $this->createServiceManagerMock();
        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, []);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithInvalidFilterManagerAsString()
    {
        // Arrange
        $container = $this->createServiceManagerMock();
        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, [
            'filter_manager' => 'my_manager',
        ]);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'filter_manager' => null,
        ], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithValidFilterManagerAsString()
    {
        // Arrange
        $container = $this->createServiceManagerMock();
        $container->has('LogFormatterManager')->willReturn(false);
        $container->has('my_manager')->willReturn(true);
        $container->get('my_manager')->willReturn(123);

        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, [
            'filter_manager' => 'my_manager',
        ]);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'filter_manager' => 123,
        ], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithoutFilterManager()
    {
        // Arrange
        $container = $this->createServiceManagerMock();
        $container->has('LogFilterManager')->willReturn(true);
        $container->get('LogFilterManager')->willReturn(123);
        $container->has('LogFormatterManager')->willReturn(false);

        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, []);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'filter_manager' => 123,
        ], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithInvalidFormatterManagerAsString()
    {
        // Arrange
        $container = $this->prophesize(ContainerInterface::class);
        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, [
            'formatter_manager' => 'my_manager',
        ]);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'formatter_manager' => null,
        ], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithValidFormatterManagerAsString()
    {
        // Arrange
        $container = $this->prophesize(ContainerInterface::class);
        $container->has('LogFilterManager')->willReturn(false);
        $container->get('my_manager')->willReturn(123);

        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, [
            'formatter_manager' => 'my_manager',
        ]);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'formatter_manager' => 123,
        ], $object->options);
    }

    /**
     * @covers \Zend\Log\Writer\Factory\WriterFactory::__invoke
     */
    public function testInvokeWithoutFormatterManager()
    {
        // Arrange
        $container = $this->prophesize(ContainerInterface::class);
        $container->has('LogFilterManager')->willReturn(true);
        $container->get('LogFilterManager')->willReturn(null);
        $container->has('LogFormatterManager')->willReturn(true);
        $container->get('LogFormatterManager')->willReturn(123);

        $factory = new WriterFactory();

        // Act
        $object = $factory($container->reveal(), InvokableObject::class, []);

        // Assert
        $this->assertInstanceOf(InvokableObject::class, $object);
        $this->assertEquals([
            'filter_manager' => null,
            'formatter_manager' => 123,
        ], $object->options);
    }
}

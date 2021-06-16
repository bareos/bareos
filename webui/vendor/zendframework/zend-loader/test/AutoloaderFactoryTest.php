<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Loader;

use PHPUnit\Framework\TestCase;
use ReflectionClass;
use Zend\Loader\AutoloaderFactory;
use Zend\Loader\Exception\InvalidArgumentException;

/**
 * @group      Loader
 */
class AutoloaderFactoryTest extends TestCase
{
    public function setUp()
    {
        // Store original autoloaders
        $this->loaders = spl_autoload_functions();
        if (! is_array($this->loaders)) {
            // spl_autoload_functions does not return empty array when no
            // autoloaders registered...
            $this->loaders = [];
        }

        // Store original include_path
        $this->includePath = get_include_path();
    }

    public function tearDown()
    {
        AutoloaderFactory::unregisterAutoloaders();
        // Restore original autoloaders
        $loaders = spl_autoload_functions();
        if (is_array($loaders)) {
            foreach ($loaders as $loader) {
                spl_autoload_unregister($loader);
            }
        }

        foreach ($this->loaders as $loader) {
            spl_autoload_register($loader);
        }

        // Restore original include_path
        set_include_path($this->includePath);
    }

    public function testRegisteringValidMapFilePopulatesAutoloader()
    {
        AutoloaderFactory::factory([
            'Zend\Loader\ClassMapAutoloader' => [
                __DIR__ . '/_files/goodmap.php',
            ],
        ]);
        $loader = AutoloaderFactory::getRegisteredAutoloader('Zend\Loader\ClassMapAutoloader');
        $map = $loader->getAutoloadMap();
        $this->assertInternalType('array', $map);
        $this->assertCount(2, $map);
    }

    /**
     * This tests checks if invalid autoloaders cause exceptions
     *
     * @expectedException InvalidArgumentException
     */
    public function testFactoryCatchesInvalidClasses()
    {
        include __DIR__ . '/_files/InvalidInterfaceAutoloader.php';
        AutoloaderFactory::factory([
            'InvalidInterfaceAutoloader' => []
        ]);
    }

    public function testFactoryDoesNotRegisterDuplicateAutoloaders()
    {
        AutoloaderFactory::factory([
            'Zend\Loader\StandardAutoloader' => [
                'namespaces' => [
                    'TestNamespace' => __DIR__ . '/TestAsset/TestNamespace',
                ],
            ],
        ]);
        $this->assertCount(1, AutoloaderFactory::getRegisteredAutoloaders());
        AutoloaderFactory::factory([
            'Zend\Loader\StandardAutoloader' => [
                'namespaces' => [
                    'ZendTest\Loader\TestAsset\TestPlugins' => __DIR__ . '/TestAsset/TestPlugins',
                ],
            ],
        ]);
        $this->assertCount(1, AutoloaderFactory::getRegisteredAutoloaders());
        $this->assertTrue(class_exists('TestNamespace\NoDuplicateAutoloadersCase'));
        $this->assertTrue(class_exists('ZendTest\Loader\TestAsset\TestPlugins\Foo'));
    }

    public function testCanUnregisterAutoloaders()
    {
        AutoloaderFactory::factory([
            'Zend\Loader\StandardAutoloader' => [
                'namespaces' => [
                    'TestNamespace' => __DIR__ . '/TestAsset/TestNamespace',
                ],
            ],
        ]);
        AutoloaderFactory::unregisterAutoloaders();
        $this->assertCount(0, AutoloaderFactory::getRegisteredAutoloaders());
    }

    public function testCanUnregisterAutoloadersByClassName()
    {
        AutoloaderFactory::factory([
            'Zend\Loader\StandardAutoloader' => [
                'namespaces' => [
                    'TestNamespace' => __DIR__ . '/TestAsset/TestNamespace',
                ],
            ],
        ]);
        AutoloaderFactory::unregisterAutoloader('Zend\Loader\StandardAutoloader');
        $this->assertCount(0, AutoloaderFactory::getRegisteredAutoloaders());
    }

    public function testCanGetValidRegisteredAutoloader()
    {
        AutoloaderFactory::factory([
            'Zend\Loader\StandardAutoloader' => [
                'namespaces' => [
                    'TestNamespace' => __DIR__ . '/TestAsset/TestNamespace',
                ],
            ],
        ]);
        $autoloader = AutoloaderFactory::getRegisteredAutoloader('Zend\Loader\StandardAutoloader');
        $this->assertInstanceOf('Zend\Loader\StandardAutoloader', $autoloader);
    }

    public function testDefaultAutoloader()
    {
        AutoloaderFactory::factory();
        $autoloader = AutoloaderFactory::getRegisteredAutoloader('Zend\Loader\StandardAutoloader');
        $this->assertInstanceOf('Zend\Loader\StandardAutoloader', $autoloader);
        $this->assertCount(1, AutoloaderFactory::getRegisteredAutoloaders());
    }

    public function testGetInvalidAutoloaderThrowsException()
    {
        $this->expectException(InvalidArgumentException::class);
        $loader = AutoloaderFactory::getRegisteredAutoloader('InvalidAutoloader');
    }

    public function testFactoryWithInvalidArgumentThrowsException()
    {
        $this->expectException(InvalidArgumentException::class);
        AutoloaderFactory::factory('InvalidArgument');
    }

    public function testFactoryWithInvalidAutoloaderClassThrowsException()
    {
        $this->expectException(InvalidArgumentException::class);
        AutoloaderFactory::factory(['InvalidAutoloader' => []]);
    }

    public function testCannotBeInstantiatedViaConstructor()
    {
        $reflection = new ReflectionClass('Zend\Loader\AutoloaderFactory');
        $constructor = $reflection->getConstructor();
        $this->assertNull($constructor);
    }

    public function testPassingNoArgumentsToFactoryInstantiatesAndRegistersStandardAutoloader()
    {
        AutoloaderFactory::factory();
        $loaders = AutoloaderFactory::getRegisteredAutoloaders();
        $this->assertCount(1, $loaders);
        $loader = array_shift($loaders);
        $this->assertInstanceOf('Zend\Loader\StandardAutoloader', $loader);

        $test  = [$loader, 'autoload'];
        $found = false;
        foreach (spl_autoload_functions() as $function) {
            if ($function === $test) {
                $found = true;
                break;
            }
        }
        $this->assertTrue($found, 'StandardAutoloader not registered with spl_autoload');
    }
}

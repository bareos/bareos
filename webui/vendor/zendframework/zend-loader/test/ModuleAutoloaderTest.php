<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Loader;

use InvalidArgumentException;
use PHPUnit\Framework\TestCase;
use Zend\Loader\ModuleAutoloader;

class ModuleAutoloaderTest extends TestCase
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

    public function testCanRegisterPathsFromConstructor()
    {
        $paths = [__DIR__ . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR];
        $loader = new ModuleAutoloader($paths);
        $registeredPaths = $loader->getPaths();
        $this->assertSame($paths, $registeredPaths);
    }

    public function testPathsNormalizedWithTrailingSlash()
    {
        $paths = [
            __DIR__ . DIRECTORY_SEPARATOR . '_files',
            __DIR__ . DIRECTORY_SEPARATOR . '_files///',
            __DIR__ . DIRECTORY_SEPARATOR . '_files\\\\',
        ];
        $loader = new ModuleAutoloader($paths);
        $registeredPaths = $loader->getPaths();
        $this->assertSame(__DIR__ . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR, $registeredPaths[0]);
        $this->assertSame(__DIR__ . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR, $registeredPaths[1]);
        $this->assertSame(__DIR__ . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR, $registeredPaths[2]);
    }

    public function testCanAutoloadModule()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $moduleClass = $loader->autoload('FooModule\Module');
        $this->assertSame('FooModule\Module', $moduleClass);
        $module = new \FooModule\Module;
        $this->assertInstanceOf('FooModule\Module', $module);
    }

    public function testCanAutoloadSubModule()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $loader->register();
        $subModule = new \FooModule\SubModule\Module;
        $this->assertInstanceOf('FooModule\SubModule\Module', $subModule);
        $loader->unregister();
    }

    public function testCanAutoloadPharModules()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $loader->register();

        $this->assertTrue(class_exists('PharModule\Module'));
        $this->assertTrue(class_exists('PharModuleTar\Module'));
        $this->assertTrue(class_exists('PharModulePharTar\Module'));
        $this->assertTrue(class_exists('PharModuleNested\Module'));

        // gzip / zip
        if (extension_loaded('zlib')) {
            // gzip
            $this->assertTrue(class_exists('PharModuleGz\Module'));
            $this->assertTrue(class_exists('PharModulePharTarGz\Module'));
            $this->assertTrue(class_exists('PharModuleTarGz\Module'));

            // zip
            $this->assertTrue(class_exists('PharModulePharZip\Module'));
            $this->assertTrue(class_exists('PharModuleZip\Module'));
        } else {
            $this->assertFalse(class_exists('PharModuleGz\Module'));
            $this->assertFalse(class_exists('PharModulePharTarGz\Module'));
            $this->assertFalse(class_exists('PharModuleTarGz\Module'));

            $this->assertFalse(class_exists('PharModulePharZip\Module'));
            $this->assertFalse(class_exists('PharModuleZip\Module'));
        }

        // bzip2
        if (extension_loaded('bzip2')) {
            $this->assertTrue(class_exists('PharModuleBz2\Module'));
            $this->assertTrue(class_exists('PharModulePharTarBz2\Module'));
            $this->assertTrue(class_exists('PharModuleTarBz2\Module'));
        } else {
            $this->assertFalse(class_exists('PharModuleBz2\Module'));
            $this->assertFalse(class_exists('PharModulePharTarBz2\Module'));
            $this->assertFalse(class_exists('PharModuleTarBz2\Module'));
        }

        $loader->unregister();
    }

    public function testCanAutoloadModulesFromWithinExecutedPhar()
    {
        $loader   = new ModuleAutoloader;

        $class    = new \ReflectionClass('Zend\Loader\ModuleAutoloader');
        $property = $class->getProperty("pharBasePath");
        $property->setAccessible(true);
        $property->setValue($loader, 'phar://' . __DIR__ . '/_files/ApplicationModulePhar.phar');

        $loader->registerPath('./module');
        $loader->register();
        $this->assertTrue(class_exists('Application\Module'));
    }

    public function testProvidesFluidInterface()
    {
        $loader = new ModuleAutoloader;
        $this->assertInstanceOf('Zend\Loader\ModuleAutoloader', $loader->setOptions(['foo']));
        $this->assertInstanceOf('Zend\Loader\ModuleAutoloader', $loader->registerPaths(['foo']));
        $this->assertInstanceOf('Zend\Loader\ModuleAutoloader', $loader->registerPath('foo'));
    }

    public function testReturnsFalseForNonModuleClass()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $moduleClass = $loader->autoload('FooModule\NotModule');
        $this->assertFalse($moduleClass);
    }

    public function testReturnsFalseForNonExistantModuleClass()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $moduleClass = $loader->autoload('NonExistantModule\Module');
        $this->assertFalse($moduleClass);
        $loader->registerPath(__DIR__ . '/_files/NonExistantModule', 'NonExistantModule');
        $moduleClass = $loader->autoload('NonExistantModule\Module');
        $this->assertFalse($moduleClass);
        $moduleClass = $loader->autoload('NoModuleClassModule\Module');
        $this->assertFalse($moduleClass);
    }

    public function testReturnsFalseForNonModulePhar()
    {
        $loader = new ModuleAutoloader;
        $loader->registerPath(__DIR__ . '/_files/');
        $moduleClass = $loader->autoload('PharModuleFake\Module');
        $moduleClass = $loader->autoload('PharModuleNestedFake\Module');
        $this->assertFalse($moduleClass);
    }

    public function testInvalidPathThrowsException()
    {
        $loader = new ModuleAutoloader;
        $this->expectException(InvalidArgumentException::class);
        $loader->registerPath(123);
    }

    public function testInvalidPathsThrowsException()
    {
        $loader = new ModuleAutoloader;
        $this->expectException(InvalidArgumentException::class);
        $loader->registerPaths(123);
    }

    public function testCanLoadModulesFromExplicitLocation()
    {
        $loader = new ModuleAutoloader([
            'My\NonmatchingModule' => __DIR__ . '/_files/NonmatchingModule',
            'PharModuleExplicit' => __DIR__ . '/_files/PharModuleExplicit.phar',
        ]);
        $loader->register();
        $this->assertTrue(class_exists('My\NonmatchingModule\Module'));
        $this->assertTrue(class_exists('PharModuleExplicit\Module'));
    }

    public function testCanLoadModulesFromClassMap()
    {
        $loader = new ModuleAutoloader();
        $loader->setModuleClassMap([
            'BarModule\Module' => __DIR__ . '/_files/BarModule/Module.php',
            'PharModuleMap\Module' => __DIR__ . '/_files/PharModuleMap.phar',
        ]);
        $loader->register();

        $this->assertTrue(class_exists('BarModule\Module'));
        $this->assertTrue(class_exists('PharModuleMap\Module'));
    }

    public function testCanLoadModulesFromNamespace()
    {
        $loader = new ModuleAutoloader([
            'FooModule\*' => __DIR__ . '/_files/FooModule',
            'FooModule' => __DIR__ . '/_files/FooModule',
        ]);
        $loader->register();
        $this->assertTrue(class_exists('FooModule\BarModule\Module'));
        $this->assertTrue(class_exists('FooModule\SubModule\Module'));
        $this->assertTrue(class_exists('FooModule\Module'));
    }
}

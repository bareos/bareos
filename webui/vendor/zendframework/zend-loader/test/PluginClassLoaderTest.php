<?php
/**
 * @see       https://github.com/zendframework/zend-loader for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-loader/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Loader;

use PHPUnit\Framework\TestCase;
use Zend\Loader\Exception\InvalidArgumentException;
use Zend\Loader\PluginClassLoader;

/**
 * @group      Loader
 */
class PluginClassLoaderTest extends TestCase
{
    /** @var PluginClassLoader */
    public $loader;

    public function setUp()
    {
        // Clear any static maps
        PluginClassLoader::addStaticMap(null);
        TestAsset\ExtendedPluginClassLoader::addStaticMap(null);

        // Create a loader instance
        $this->loader = new PluginClassLoader();
    }

    public function testPluginClassLoaderHasNoAssociationsByDefault()
    {
        $plugins = $this->loader->getRegisteredPlugins();
        $this->assertEmpty($plugins);
    }

    public function testRegisterPluginRegistersShortNameClassNameAssociation()
    {
        $this->loader->registerPlugin('loader', __CLASS__);
        $plugins = $this->loader->getRegisteredPlugins();
        $this->assertArrayHasKey('loader', $plugins);
        $this->assertEquals(__CLASS__, $plugins['loader']);
    }

    public function testCallingRegisterPluginWithAnExistingPluginNameOverwritesThatMapAssociation()
    {
        $this->testRegisterPluginRegistersShortNameClassNameAssociation();
        $this->loader->registerPlugin('loader', 'Zend\Loader\PluginClassLoader');
        $plugins = $this->loader->getRegisteredPlugins();
        $this->assertArrayHasKey('loader', $plugins);
        $this->assertEquals('Zend\Loader\PluginClassLoader', $plugins['loader']);
    }

    public function testCallingRegisterPluginsWithInvalidStringMapRaisesException()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->loader->registerPlugins('__foobar__');
    }

    public function testCallingRegisterPluginsWithStringMapResolvingToNonTraversableClassRaisesException()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->loader->registerPlugins('stdClass');
    }

    public function testCallingRegisterPluginsWithValidStringMapResolvingToTraversableClassRegistersPlugins()
    {
        $this->loader->registerPlugins('ZendTest\Loader\TestAsset\TestPluginMap');
        $pluginMap = new TestAsset\TestPluginMap;
        $this->assertEquals($pluginMap->map, $this->loader->getRegisteredPlugins());
    }

    /**
     * @dataProvider invalidMaps
     */
    public function testCallingRegisterPluginsWithNonArrayNonStringNonTraversableValueRaisesException($arg)
    {
        $this->expectException(InvalidArgumentException::class);
        $this->loader->registerPlugins($arg);
    }

    public function invalidMaps()
    {
        return [
            [null],
            [true],
            [1],
            [1.0],
            [new \stdClass],
        ];
    }

    public function testCallingRegisterPluginsWithArrayRegistersMap()
    {
        $map = ['test' => __CLASS__];
        $this->loader->registerPlugins($map);
        $test = $this->loader->getRegisteredPlugins();
        $this->assertEquals($map, $test);
    }

    public function testCallingRegisterPluginsWithTraversableObjectRegistersMap()
    {
        $map = new TestAsset\TestPluginMap();
        $this->loader->registerPlugins($map);
        $test = $this->loader->getRegisteredPlugins();
        $this->assertEquals($map->map, $test);
    }

    public function testUnregisterPluginRemovesPluginFromMap()
    {
        $map = new TestAsset\TestPluginMap();
        $this->loader->registerPlugins($map);

        $this->loader->unregisterPlugin('test');

        $test = $this->loader->getRegisteredPlugins();
        $this->assertArrayNotHasKey('test', $test);
    }

    public function testIsLoadedReturnsFalseIfPluginIsNotInMap()
    {
        $this->assertFalse($this->loader->isLoaded('test'));
    }

    public function testIsLoadedReturnsTrueIfPluginIsInMap()
    {
        $this->loader->registerPlugin('test', __CLASS__);
        $this->assertTrue($this->loader->isLoaded('test'));
    }

    public function testGetClassNameReturnsFalseIfPluginIsNotInMap()
    {
        $this->assertFalse($this->loader->getClassName('test'));
    }

    public function testGetClassNameReturnsClassNameIfPluginIsInMap()
    {
        $this->loader->registerPlugin('test', __CLASS__);
        $this->assertEquals(__CLASS__, $this->loader->getClassName('test'));
    }

    public function testLoadReturnsFalseIfPluginIsNotInMap()
    {
        $this->assertFalse($this->loader->load('test'));
    }

    public function testLoadReturnsClassNameIfPluginIsInMap()
    {
        $this->loader->registerPlugin('test', __CLASS__);
        $this->assertEquals(__CLASS__, $this->loader->load('test'));
    }

    public function testIteratingLoaderIteratesPluginMap()
    {
        $map = new TestAsset\TestPluginMap();
        $this->loader->registerPlugins($map);
        $test = [];
        foreach ($this->loader as $name => $class) {
            $test[$name] = $class;
        }

        $this->assertEquals($map->map, $test);
    }

    public function testPluginRegistrationIsCaseInsensitive()
    {
        $map = [
            'foo' => __CLASS__,
            'FOO' => __NAMESPACE__ . '\TestAsset\TestPluginMap',
        ];
        $this->loader->registerPlugins($map);
        $this->assertEquals($map['FOO'], $this->loader->getClassName('foo'));
    }

    public function testAddingStaticMapDoesNotAffectExistingInstances()
    {
        PluginClassLoader::addStaticMap([
            'test' => __CLASS__,
        ]);
        $this->assertFalse($this->loader->getClassName('test'));
    }

    public function testAllowsSettingStaticMapForSeedingInstance()
    {
        PluginClassLoader::addStaticMap([
            'test' => __CLASS__,
        ]);
        $loader = new PluginClassLoader();
        $this->assertEquals(__CLASS__, $loader->getClassName('test'));
    }

    public function testPassingNullToStaticMapClearsMap()
    {
        $this->testAllowsSettingStaticMapForSeedingInstance();
        PluginClassLoader::addStaticMap(null);
        $loader = new PluginClassLoader();
        $this->assertFalse($loader->getClassName('test'));
    }

    public function testAllowsPassingTraversableObjectToStaticMap()
    {
        $map = new \ArrayObject([
            'test' => __CLASS__,
        ]);
        PluginClassLoader::addStaticMap($map);
        $loader = new PluginClassLoader();
        $this->assertEquals(__CLASS__, $loader->getClassName('test'));
    }

    public function testMultipleCallsToAddStaticMapMergeMap()
    {
        PluginClassLoader::addStaticMap([
            'test' => __CLASS__,
        ]);
        PluginClassLoader::addStaticMap([
            'loader' => 'Zend\Loader\PluginClassLoader',
        ]);
        $loader = new PluginClassLoader();
        $this->assertEquals(__CLASS__, $loader->getClassName('test'));
        $this->assertEquals('Zend\Loader\PluginClassLoader', $loader->getClassName('loader'));
    }

    public function testStaticMapUsesLateStaticBinding()
    {
        TestAsset\ExtendedPluginClassLoader::addStaticMap(['test' => __CLASS__]);
        $loader = new PluginClassLoader();
        $this->assertFalse($loader->getClassName('test'));
        $loader = new TestAsset\ExtendedPluginClassLoader();
        $this->assertEquals(__CLASS__, $loader->getClassName('test'));
    }

    public function testMapPrecedenceIsExplicitTrumpsConstructorTrumpsStaticTrumpsInternal()
    {
        $loader = new TestAsset\ExtendedPluginClassLoader();
        $this->assertEquals('Zend\Loader\PluginClassLoader', $loader->getClassName('loader'));

        TestAsset\ExtendedPluginClassLoader::addStaticMap(['loader' => __CLASS__]);
        $loader = new TestAsset\ExtendedPluginClassLoader();
        $this->assertEquals(__CLASS__, $loader->getClassName('loader'));

        $loader = new TestAsset\ExtendedPluginClassLoader(
            ['loader' => 'ZendTest\Loader\TestAsset\ExtendedPluginClassLoader']
        );
        $this->assertEquals('ZendTest\Loader\TestAsset\ExtendedPluginClassLoader', $loader->getClassName('loader'));

        $loader->registerPlugin('loader', __CLASS__);
        $this->assertEquals(__CLASS__, $loader->getClassName('loader'));
    }

    public function testRegisterPluginsCanAcceptArrayElementWithClassNameProvidingAMap()
    {
        $pluginMap = new TestAsset\TestPluginMap;
        $this->loader->registerPlugins(['ZendTest\Loader\TestAsset\TestPluginMap']);
        $this->assertEquals($pluginMap->map, $this->loader->getRegisteredPlugins());
    }

    public function testRegisterPluginsCanAcceptArrayElementWithObjectProvidingAMap()
    {
        $pluginMap = new TestAsset\TestPluginMap;
        $this->loader->registerPlugins([$pluginMap]);
        $this->assertEquals($pluginMap->map, $this->loader->getRegisteredPlugins());
    }
}

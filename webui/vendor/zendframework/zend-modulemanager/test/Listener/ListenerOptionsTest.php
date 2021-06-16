<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use InvalidArgumentException;
use PHPUnit\Framework\TestCase;
use Zend\Config\Config;
use Zend\ModuleManager\Listener\ListenerOptions;

/**
 * @covers \Zend\ModuleManager\Listener\ListenerOptions
 */
class ListenerOptionsTest extends TestCase
{
    public function testCanConfigureWithArrayInConstructor()
    {
        $options = new ListenerOptions([
            'cache_dir'               => __DIR__,
            'config_cache_enabled'    => true,
            'config_cache_key'        => 'foo',
            'module_paths'            => ['module', 'paths'],
            'config_glob_paths'       => ['glob', 'paths'],
            'config_static_paths'       => ['static', 'custom_paths'],
        ]);
        $this->assertSame($options->getCacheDir(), __DIR__);
        $this->assertTrue($options->getConfigCacheEnabled());
        $this->assertNotNull(strstr($options->getConfigCacheFile(), __DIR__));
        $this->assertNotNull(strstr($options->getConfigCacheFile(), '.php'));
        $this->assertSame('foo', $options->getConfigCacheKey());
        $this->assertSame(['module', 'paths'], $options->getModulePaths());
        $this->assertSame(['glob', 'paths'], $options->getConfigGlobPaths());
        $this->assertSame(['static', 'custom_paths'], $options->getConfigStaticPaths());
    }

    /**
     * @group 6552
     */
    public function testConfigCacheFileWithEmptyCacheKey()
    {
        $options = new ListenerOptions([
           'cache_dir'               => __DIR__,
           'config_cache_enabled'    => true,
           'module_paths'            => ['module', 'paths'],
           'config_glob_paths'       => ['glob', 'paths'],
           'config_static_paths'     => ['static', 'custom_paths'],
        ]);

        $this->assertEquals(__DIR__ . '/module-config-cache.php', $options->getConfigCacheFile());
        $options->setConfigCacheKey('foo');
        $this->assertEquals(__DIR__ . '/module-config-cache.foo.php', $options->getConfigCacheFile());
    }

    /**
     * @group 6552
     */
    public function testModuleMapCacheFileWithEmptyCacheKey()
    {
        $options = new ListenerOptions([
           'cache_dir'                => __DIR__,
           'module_map_cache_enabled' => true,
           'module_paths'             => ['module', 'paths'],
           'config_glob_paths'        => ['glob', 'paths'],
           'config_static_paths'      => ['static', 'custom_paths'],
        ]);

        $this->assertEquals(__DIR__ . '/module-classmap-cache.php', $options->getModuleMapCacheFile());
        $options->setModuleMapCacheKey('foo');
        $this->assertEquals(__DIR__ . '/module-classmap-cache.foo.php', $options->getModuleMapCacheFile());
    }

    public function testCanAccessKeysAsProperties()
    {
        $options = new ListenerOptions([
            'cache_dir'               => __DIR__,
            'config_cache_enabled'    => true,
            'config_cache_key'        => 'foo',
            'module_paths'            => ['module', 'paths'],
            'config_glob_paths'       => ['glob', 'paths'],
            'config_static_paths'       => ['static', 'custom_paths'],
        ]);
        $this->assertSame($options->cache_dir, __DIR__);
        $options->cache_dir = 'foo';
        $this->assertSame($options->cache_dir, 'foo');
        $this->assertTrue(isset($options->cache_dir));
        unset($options->cache_dir);
        $this->assertFalse(isset($options->cache_dir));

        $this->assertTrue($options->config_cache_enabled);
        $options->config_cache_enabled = false;
        $this->assertFalse($options->config_cache_enabled);
        $this->assertEquals('foo', $options->config_cache_key);
        $this->assertSame(['module', 'paths'], $options->module_paths);
        $this->assertSame(['glob', 'paths'], $options->config_glob_paths);
        $this->assertSame(['static', 'custom_paths'], $options->config_static_paths);
    }

    public function testSetModulePathsAcceptsConfigOrTraverable()
    {
        $config = new Config([__DIR__]);
        $options = new ListenerOptions;
        $options->setModulePaths($config);
        $this->assertSame($config, $options->getModulePaths());
    }

    public function testSetModulePathsThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $options = new ListenerOptions;
        $options->setModulePaths('asd');
    }

    public function testSetConfigGlobPathsAcceptsConfigOrTraverable()
    {
        $config = new Config([__DIR__]);
        $options = new ListenerOptions;
        $options->setConfigGlobPaths($config);
        $this->assertSame($config, $options->getConfigGlobPaths());
    }

    public function testSetConfigGlobPathsThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $options = new ListenerOptions;
        $options->setConfigGlobPaths('asd');
    }

    public function testSetConfigStaticPathsThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $options = new ListenerOptions;
        $options->setConfigStaticPaths('asd');
    }

    public function testSetExtraConfigAcceptsArrayOrTraverable()
    {
        $array = [__DIR__];
        $traversable = new Config($array);
        $options = new ListenerOptions;

        $this->assertSame($options, $options->setExtraConfig($array));
        $this->assertSame($array, $options->getExtraConfig());

        $this->assertSame($options, $options->setExtraConfig($traversable));
        $this->assertSame($traversable, $options->getExtraConfig());
    }

    public function testSetExtraConfigThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $options = new ListenerOptions;
        $options->setExtraConfig('asd');
    }
}

<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Plugin;

use PHPUnit\Framework\TestCase;
use Zend\ServiceManager\ServiceManager;
use Zend\Cache\Storage\PluginManager;

/**
 * PHPUnit test case
 */

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Plugin\PluginOptions<extended>
 */
abstract class CommonPluginTest extends TestCase
{
    // @codingStandardsIgnoreStart
    /**
     * The storage plugin
     *
     * @var \Zend\Cache\Storage\Plugin\PluginInterface
     */
    protected $_plugin;
    // @codingStandardsIgnoreEnd

    /**
     * A data provider for common storage plugin names
     */
    abstract public function getCommonPluginNamesProvider();

    /**
     * @dataProvider getCommonPluginNamesProvider
     */
    public function testPluginManagerWithCommonNames($commonPluginName)
    {
        $pluginManager = new PluginManager(new ServiceManager);
        $this->assertTrue(
            $pluginManager->has($commonPluginName),
            "Storage plugin name '{$commonPluginName}' not found in storage plugin manager"
        );
    }

    public function testOptionObjectAvailable()
    {
        $options = $this->_plugin->getOptions();
        $this->assertInstanceOf('Zend\Cache\Storage\Plugin\PluginOptions', $options);
    }

    public function testOptionsGetAndSetDefault()
    {
        $options = $this->_plugin->getOptions();
        $this->_plugin->setOptions($options);
        $this->assertSame($options, $this->_plugin->getOptions());
    }
}

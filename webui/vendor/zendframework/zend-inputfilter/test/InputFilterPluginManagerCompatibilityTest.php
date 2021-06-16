<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use PHPUnit\Framework\TestCase;
use Zend\InputFilter\Exception\RuntimeException;
use Zend\InputFilter\InputFilterPluginManager;
use Zend\ServiceManager\Config;
use Zend\ServiceManager\ServiceManager;
use Zend\ServiceManager\Test\CommonPluginManagerTrait;

class InputFilterPluginManagerCompatibilityTest extends TestCase
{
    use CommonPluginManagerTrait;

    public function testInstanceOfMatches()
    {
        $this->markTestSkipped("InputFilterPluginManager accepts multiple instances");
    }

    protected function getPluginManager()
    {
        return new InputFilterPluginManager(new ServiceManager());
    }

    protected function getV2InvalidPluginException()
    {
        return RuntimeException::class;
    }

    protected function getInstanceOf()
    {
        // InputFilterManager accepts multiple instance types
        return;
    }

    public function testConstructorArgumentsAreOptionalUnderV2()
    {
        $plugins = $this->getPluginManager();
        if (method_exists($plugins, 'configure')) {
            $this->markTestSkipped('zend-servicemanager v3 plugin managers require a container argument');
        }

        $plugins = new InputFilterPluginManager();
        $this->assertInstanceOf(InputFilterPluginManager::class, $plugins);
    }

    public function testConstructorAllowsConfigInstanceAsFirstArgumentUnderV2()
    {
        $plugins = $this->getPluginManager();
        if (method_exists($plugins, 'configure')) {
            $this->markTestSkipped('zend-servicemanager v3 plugin managers require a container argument');
        }

        $plugins = new InputFilterPluginManager(new Config([]));
        $this->assertInstanceOf(InputFilterPluginManager::class, $plugins);
    }
}

<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager;

use PHPUnit\Framework\TestCase;
use stdClass;
use Zend\ModuleManager\Exception;
use Zend\ModuleManager\Listener\ConfigListener;
use Zend\ModuleManager\ModuleEvent;

/**
 * @covers \Zend\ModuleManager\ModuleEvent
 */
class ModuleEventTest extends TestCase
{
    /**
     * @var ModuleEvent
     */
    protected $event;

    protected function setUp()
    {
        $this->event = new ModuleEvent();
    }

    public function testCanRetrieveModuleViaGetter()
    {
        $module = new stdClass;
        $this->event->setModule($module);
        $test = $this->event->getModule();
        $this->assertSame($module, $test);
    }

    public function testPassingNonObjectToSetModuleRaisesException()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->event->setModule('foo');
    }

    public function testCanRetrieveModuleNameViaGetter()
    {
        $moduleName = 'MyModule';
        $this->event->setModuleName($moduleName);
        $test = $this->event->getModuleName();
        $this->assertSame($moduleName, $test);
    }

    public function testPassingNonStringToSetModuleNameRaisesException()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->event->setModuleName(new stdClass);
    }

    public function testSettingConfigListenerProxiesToParameters()
    {
        $configListener = new ConfigListener;
        $this->event->setConfigListener($configListener);
        $test = $this->event->getParam('configListener');
        $this->assertSame($configListener, $test);
    }

    public function testCanRetrieveConfigListenerViaGetter()
    {
        $configListener = new ConfigListener;
        $this->event->setConfigListener($configListener);
        $test = $this->event->getConfigListener();
        $this->assertSame($configListener, $test);
    }
}

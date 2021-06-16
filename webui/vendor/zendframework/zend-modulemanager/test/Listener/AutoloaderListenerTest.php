<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use Zend\ModuleManager\Listener\AutoloaderListener;
use Zend\ModuleManager\Listener\ModuleResolverListener;
use Zend\ModuleManager\ModuleEvent;
use Zend\ModuleManager\ModuleManager;

/**
 * @covers \Zend\ModuleManager\Listener\AbstractListener
 * @covers \Zend\ModuleManager\Listener\AutoloaderListener
 */
class AutoloaderListenerTest extends AbstractListenerTestCase
{
    /**
     * @var ModuleManager
     */
    protected $moduleManager;

    protected function setUp()
    {
        $this->moduleManager = new ModuleManager([]);
        $events = $this->moduleManager->getEventManager();
        $events->attach(ModuleEvent::EVENT_LOAD_MODULE_RESOLVE, new ModuleResolverListener, 1000);
        $events->attach(ModuleEvent::EVENT_LOAD_MODULE, new AutoloaderListener, 2000);
    }

    public function testAutoloadersRegisteredByAutoloaderListener()
    {
        $moduleManager = $this->moduleManager;
        $moduleManager->setModules(['ListenerTestModule']);
        $moduleManager->loadModules();
        $modules = $moduleManager->getLoadedModules();
        $this->assertTrue($modules['ListenerTestModule']->getAutoloaderConfigCalled);
        $this->assertTrue(class_exists('Foo\Bar'));
    }
    // @codingStandardsIgnoreStart
    public function testAutoloadersRegisteredIfModuleDoesNotInheritAutoloaderProviderInterfaceButDefinesGetAutoloaderConfigMethod()
    {
        $moduleManager = $this->moduleManager;
        $moduleManager->setModules(['NotAutoloaderModule']);
        $moduleManager->loadModules();
        $modules = $moduleManager->getLoadedModules();
        $this->assertTrue($modules['NotAutoloaderModule']->getAutoloaderConfigCalled);
        $this->assertTrue(class_exists('Foo\Bar'));
    }
   // @codingStandardsIgnoreEnd
}

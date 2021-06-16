<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use ReflectionClass;
use Zend\EventManager\EventManager;
use Zend\EventManager\SharedEventManager;
use Zend\ModuleManager\Listener\ModuleResolverListener;
use Zend\ModuleManager\Listener\OnBootstrapListener;
use Zend\ModuleManager\ModuleEvent;
use Zend\ModuleManager\ModuleManager;
use Zend\Mvc\Application;
use ZendTest\ModuleManager\TestAsset\MockApplication;

/**
 * @covers \Zend\ModuleManager\Listener\AbstractListener
 * @covers \Zend\ModuleManager\Listener\OnBootstrapListener
 */
class OnBootstrapListenerTest extends AbstractListenerTestCase
{
    /**
     * @var Application
     */
    protected $application;

    /**
     * @var ModuleManager
     */
    protected $moduleManager;

    protected function setUp()
    {
        $sharedEvents = new SharedEventManager();
        $events       = new EventManager($sharedEvents);
        $this->moduleManager = new ModuleManager([]);
        $this->moduleManager->setEventManager($events);

        $events->attach(ModuleEvent::EVENT_LOAD_MODULE_RESOLVE, new ModuleResolverListener, 1000);
        $events->attach(ModuleEvent::EVENT_LOAD_MODULE, new OnBootstrapListener, 1000);

        $this->application = new MockApplication;
        $appEvents         = $this->createEventManager($sharedEvents);
        $appEvents->setIdentifiers([
            'Zend\Mvc\Application',
            'ZendTest\Module\TestAsset\MockApplication',
            'application',
        ]);

        $this->application->setEventManager($appEvents);
    }

    public function createEventManager(SharedEventManager $sharedEvents)
    {
        $r = new ReflectionClass(EventManager::class);
        if ($r->hasMethod('setSharedManager')) {
            $events = new EventManager();
            $events->setSharedManager($sharedEvents);
            return $events;
        }

        return new EventManager($sharedEvents);
    }

    public function testOnBootstrapMethodCalledByOnBootstrapListener()
    {
        $moduleManager = $this->moduleManager;
        $moduleManager->setModules(['ListenerTestModule']);
        $moduleManager->loadModules();
        $this->application->bootstrap();
        $modules = $moduleManager->getLoadedModules();
        $this->assertTrue($modules['ListenerTestModule']->onBootstrapCalled);
    }
}

<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use Zend\EventManager\Test\EventListenerIntrospectionTrait;
use Zend\ModuleManager\Listener\DefaultListenerAggregate;
use Zend\ModuleManager\Listener\ListenerOptions;
use Zend\ModuleManager\ModuleManager;

/**
 * @covers \Zend\ModuleManager\Listener\AbstractListener
 * @covers \Zend\ModuleManager\Listener\DefaultListenerAggregate
 */
class DefaultListenerAggregateTest extends AbstractListenerTestCase
{
    use EventListenerIntrospectionTrait;

    /**
     * @var DefaultListenerAggregate
     */
    protected $defaultListeners;

    protected function setUp()
    {
        $this->defaultListeners = new DefaultListenerAggregate(
            new ListenerOptions([
                'module_paths'         => [
                    realpath(__DIR__ . '/TestAsset'),
                ],
            ])
        );
    }

    public function testDefaultListenerAggregateCanAttachItself()
    {
        $moduleManager = new ModuleManager(['ListenerTestModule']);
        (new DefaultListenerAggregate)->attach($moduleManager->getEventManager());

        $events = $this->getEventsFromEventManager($moduleManager->getEventManager());
        $expectedEvents = [
            'loadModules' => [
                'Zend\Loader\ModuleAutoloader',
                'config-pre' => 'Zend\ModuleManager\Listener\ConfigListener',
                'config-post' => 'Zend\ModuleManager\Listener\ConfigListener',
                'Zend\ModuleManager\Listener\LocatorRegistrationListener',
                'Zend\ModuleManager\ModuleManager',
            ],
            'loadModule.resolve' => [
                'Zend\ModuleManager\Listener\ModuleResolverListener',
            ],
            'loadModule' => [
                'Zend\ModuleManager\Listener\AutoloaderListener',
                'Zend\ModuleManager\Listener\ModuleDependencyCheckerListener',
                'Zend\ModuleManager\Listener\InitTrigger',
                'Zend\ModuleManager\Listener\OnBootstrapListener',
                'Zend\ModuleManager\Listener\ConfigListener',
                'Zend\ModuleManager\Listener\LocatorRegistrationListener',
            ],
        ];
        foreach ($expectedEvents as $event => $expectedListeners) {
            $this->assertContains($event, $events);
            $count = 0;
            foreach ($this->getListenersForEvent($event, $moduleManager->getEventManager()) as $listener) {
                if (is_array($listener)) {
                    $listener = $listener[0];
                }
                $listenerClass = get_class($listener);
                $this->assertContains($listenerClass, $expectedListeners);
                $count += 1;
            }

            $this->assertSame(count($expectedListeners), $count);
        }
    }

    public function testDefaultListenerAggregateCanDetachItself()
    {
        $listenerAggregate = new DefaultListenerAggregate;
        $moduleManager     = new ModuleManager(['ListenerTestModule']);
        $events            = $moduleManager->getEventManager();

        $this->assertEquals(1, count($this->getEventsFromEventManager($events)));

        $listenerAggregate->attach($events);
        $this->assertEquals(4, count($this->getEventsFromEventManager($events)));

        $listenerAggregate->detach($events);
        $this->assertEquals(1, count($this->getEventsFromEventManager($events)));
    }

    public function testDefaultListenerAggregateSkipsAutoloadingListenersIfZendLoaderIsNotUsed()
    {
        $moduleManager = new ModuleManager(['ListenerTestModule']);
        $eventManager = $moduleManager->getEventManager();
        $listenerAggregate = new DefaultListenerAggregate(new ListenerOptions([
            'use_zend_loader' => false,
        ]));
        $listenerAggregate->attach($eventManager);

        $events = $this->getEventsFromEventManager($eventManager);
        $expectedEvents = [
            'loadModules' => [
                'config-pre' => 'Zend\ModuleManager\Listener\ConfigListener',
                'config-post' => 'Zend\ModuleManager\Listener\ConfigListener',
                'Zend\ModuleManager\Listener\LocatorRegistrationListener',
                'Zend\ModuleManager\ModuleManager',
            ],
            'loadModule.resolve' => [
                'Zend\ModuleManager\Listener\ModuleResolverListener',
            ],
            'loadModule' => [
                'Zend\ModuleManager\Listener\ModuleDependencyCheckerListener',
                'Zend\ModuleManager\Listener\InitTrigger',
                'Zend\ModuleManager\Listener\OnBootstrapListener',
                'Zend\ModuleManager\Listener\ConfigListener',
                'Zend\ModuleManager\Listener\LocatorRegistrationListener',
            ],
        ];
        foreach ($expectedEvents as $event => $expectedListeners) {
            $this->assertContains($event, $events);
            $count = 0;
            foreach ($this->getListenersForEvent($event, $eventManager) as $listener) {
                if (is_array($listener)) {
                    $listener = $listener[0];
                }
                $listenerClass = get_class($listener);
                $this->assertContains($listenerClass, $expectedListeners);
                $count += 1;
            }
            $this->assertSame(count($expectedListeners), $count);
        }
    }
}

<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\Listener;

use Zend\EventManager\Test\EventListenerIntrospectionTrait;
use Zend\ModuleManager\Listener\ListenerOptions;
use Zend\ModuleManager\Listener\ModuleLoaderListener;
use Zend\ModuleManager\Listener\ModuleResolverListener;
use Zend\ModuleManager\ModuleEvent;
use Zend\ModuleManager\ModuleManager;
use ZendTest\ModuleManager\SetUpCacheDirTrait;

/**
 * @covers \Zend\ModuleManager\Listener\AbstractListener
 * @covers \Zend\ModuleManager\Listener\ModuleLoaderListener
 */
class ModuleLoaderListenerTest extends AbstractListenerTestCase
{
    use EventListenerIntrospectionTrait;
    use SetUpCacheDirTrait;

    /**
     * @var ModuleManager
     */
    protected $moduleManager;

    protected function setUp()
    {
        $this->moduleManager = new ModuleManager([]);
        $this->moduleManager->getEventManager()->attach(
            ModuleEvent::EVENT_LOAD_MODULE_RESOLVE,
            new ModuleResolverListener,
            1000
        );
    }

    public function testModuleLoaderListenerFunctionsAsAggregateListenerEnabledCache()
    {
        $options = new ListenerOptions([
            'cache_dir'                => $this->tmpdir,
            'module_map_cache_enabled' => true,
            'module_map_cache_key'     => 'foo',
        ]);

        $moduleLoaderListener = new ModuleLoaderListener($options);

        $moduleManager = $this->moduleManager;
        $events        = $moduleManager->getEventManager();

        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(1, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(0, $listeners);

        $moduleLoaderListener->attach($events);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(2, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(1, $listeners);
    }

    public function testModuleLoaderListenerFunctionsAsAggregateListenerDisabledCache()
    {
        $options = new ListenerOptions([
            'cache_dir' => $this->tmpdir,
        ]);

        $moduleLoaderListener = new ModuleLoaderListener($options);

        $moduleManager = $this->moduleManager;
        $events        = $moduleManager->getEventManager();

        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(1, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(0, $listeners);

        $moduleLoaderListener->attach($events);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(2, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(0, $listeners);
    }

    public function testModuleLoaderListenerFunctionsAsAggregateListenerHasCache()
    {
        $options = new ListenerOptions([
            'cache_dir'                => $this->tmpdir,
            'module_map_cache_key'     => 'foo',
            'module_map_cache_enabled' => true,
        ]);

        file_put_contents($options->getModuleMapCacheFile(), '<' . '?php return array();');

        $moduleLoaderListener = new ModuleLoaderListener($options);

        $moduleManager = $this->moduleManager;
        $events        = $moduleManager->getEventManager();

        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(1, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(0, $listeners);

        $moduleLoaderListener->attach($events);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES, $events));
        $this->assertCount(2, $listeners);
        $listeners     = iterator_to_array($this->getListenersForEvent(ModuleEvent::EVENT_LOAD_MODULES_POST, $events));
        $this->assertCount(0, $listeners);
    }
}

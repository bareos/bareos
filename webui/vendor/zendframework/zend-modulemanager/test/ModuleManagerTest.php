<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager;

use InvalidArgumentException;
use PHPUnit\Framework\TestCase;
use RuntimeException;
use stdClass;
use Zend\EventManager\EventManager;
use Zend\EventManager\SharedEventManager;
use Zend\ModuleManager\Exception;
use Zend\ModuleManager\Listener\DefaultListenerAggregate;
use Zend\ModuleManager\Listener\ListenerOptions;
use Zend\ModuleManager\ModuleEvent;
use Zend\ModuleManager\ModuleManager;

/**
 * @covers \Zend\ModuleManager\ModuleManager
 */
class ModuleManagerTest extends TestCase
{
    use ResetAutoloadFunctionsTrait;
    use SetUpCacheDirTrait;

    /**
     * @var DefaultListenerAggregate
     */
    protected $defaultListeners;

    protected function setUp()
    {
        $this->sharedEvents = new SharedEventManager;
        $this->events       = new EventManager($this->sharedEvents);
        $this->defaultListeners = new DefaultListenerAggregate(
            new ListenerOptions([
                'module_paths' => [
                    realpath(__DIR__ . '/TestAsset'),
                ],
            ])
        );
    }

    public function testEventManagerIdentifiers()
    {
        $moduleManager = new ModuleManager([]);
        $identifiers = $moduleManager->getEventManager()->getIdentifiers();
        $expected    = ['Zend\ModuleManager\ModuleManager', 'module_manager'];
        $this->assertEquals($expected, array_values($identifiers));
    }

    public function testCanLoadSomeModule()
    {
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['SomeModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();
        $loadedModules = $moduleManager->getLoadedModules();
        $this->assertInstanceOf('SomeModule\Module', $loadedModules['SomeModule']);
        $config = $configListener->getMergedConfig();
        $this->assertSame($config->some, 'thing', var_export($config, true));
    }

    public function testCanLoadMultipleModules()
    {
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['BarModule', 'BazModule', 'SubModule\Sub'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();
        $loadedModules = $moduleManager->getLoadedModules();
        $this->assertInstanceOf('BarModule\Module', $loadedModules['BarModule']);
        $this->assertInstanceOf('BazModule\Module', $loadedModules['BazModule']);
        $this->assertInstanceOf('SubModule\Sub\Module', $loadedModules['SubModule\Sub']);
        $this->assertInstanceOf('BarModule\Module', $moduleManager->getModule('BarModule'));
        $this->assertInstanceOf('BazModule\Module', $moduleManager->getModule('BazModule'));
        $this->assertInstanceOf('SubModule\Sub\Module', $moduleManager->getModule('SubModule\Sub'));
        $this->assertNull($moduleManager->getModule('NotLoaded'));
        $config = $configListener->getMergedConfig();
        $this->assertSame('foo', $config->bar);
        $this->assertSame('bar', $config->baz);
    }

    public function testModuleLoadingBehavior()
    {
        $moduleManager = new ModuleManager(['BarModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $modules = $moduleManager->getLoadedModules();
        $this->assertSame(0, count($modules));
        $modules = $moduleManager->getLoadedModules(true);
        $this->assertSame(1, count($modules));
        $moduleManager->loadModules(); // should not cause any problems
        $moduleManager->loadModule('BarModule'); // should not cause any problems
        $modules = $moduleManager->getLoadedModules(true); // BarModule already loaded so nothing happens
        $this->assertSame(1, count($modules));
    }

    public function testConstructorThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $moduleManager = new ModuleManager('stringShouldBeArray', $this->events);
    }

    public function testNotFoundModuleThrowsRuntimeException()
    {
        $this->expectException(RuntimeException::class);
        $moduleManager = new ModuleManager(['NotFoundModule'], $this->events);
        $moduleManager->loadModules();
    }

    public function testCanLoadModuleDuringTheLoadModuleEvent()
    {
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['LoadOtherModule', 'BarModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();

        $config = $configListener->getMergedConfig();
        $this->assertTrue(isset($config['loaded']));
        $this->assertSame('oh, yeah baby!', $config['loaded']);
    }

    /**
     * @group 5651
     */
    public function testLoadingModuleFromAnotherModuleDemonstratesAppropriateSideEffects()
    {
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['LoadOtherModule', 'BarModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();

        $config = $configListener->getMergedConfig();
        $this->assertTrue(isset($config['baz']));
        $this->assertSame('bar', $config['baz']);
    }

    /**
     * @group 5651
     * @group 5948
     */
    public function testLoadingModuleFromAnotherModuleDoesNotInfiniteLoop()
    {
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['LoadBarModule', 'LoadFooModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();

        $config = $configListener->getMergedConfig();

        $this->assertTrue(isset($config['bar']));
        $this->assertSame('bar', $config['bar']);

        $this->assertTrue(isset($config['foo']));
        $this->assertSame('bar', $config['foo']);
    }

    public function testModuleIsMarkedAsLoadedWhenLoadModuleEventIsTriggered()
    {
        $test          = new stdClass;
        $moduleManager = new ModuleManager(['BarModule'], $this->events);
        $events        = $this->events;
        $this->defaultListeners->attach($events);
        $events->attach(ModuleEvent::EVENT_LOAD_MODULE, function (ModuleEvent $e) use ($test) {
            $test->modules = $e->getTarget()->getLoadedModules(false);
        });

        $moduleManager->loadModules();

        $this->assertTrue(isset($test->modules));
        $this->assertArrayHasKey('BarModule', $test->modules);
        $this->assertInstanceOf('BarModule\Module', $test->modules['BarModule']);
    }

    public function testCanLoadSomeObjectModule()
    {
        require_once __DIR__ . '/TestAsset/SomeModule/Module.php';
        require_once __DIR__ . '/TestAsset/SubModule/Sub/Module.php';
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager([
            'SomeModule' => new \SomeModule\Module(),
            'SubModule' => new \SubModule\Sub\Module(),
        ], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();
        $loadedModules = $moduleManager->getLoadedModules();
        $this->assertInstanceOf('SomeModule\Module', $loadedModules['SomeModule']);
        $config = $configListener->getMergedConfig();
        $this->assertSame($config->some, 'thing');
    }

    public function testCanLoadMultipleModulesObjectWithString()
    {
        require_once __DIR__ . '/TestAsset/SomeModule/Module.php';
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager(['SomeModule' => new \SomeModule\Module(), 'BarModule'], $this->events);
        $this->defaultListeners->attach($this->events);
        $moduleManager->loadModules();
        $loadedModules = $moduleManager->getLoadedModules();
        $this->assertInstanceOf('SomeModule\Module', $loadedModules['SomeModule']);
        $config = $configListener->getMergedConfig();
        $this->assertSame($config->some, 'thing');
    }

    public function testCanNotLoadSomeObjectModuleWithoutIdentifier()
    {
        require_once __DIR__ . '/TestAsset/SomeModule/Module.php';
        $configListener = $this->defaultListeners->getConfigListener();
        $moduleManager  = new ModuleManager([new \SomeModule\Module()], $this->events);
        $this->defaultListeners->attach($this->events);
        $this->expectException(Exception\RuntimeException::class);
        $moduleManager->loadModules();
    }

    public function testSettingEventInjectsModuleManagerAsTarget()
    {
        $moduleManager = new ModuleManager([]);
        $event = new ModuleEvent();

        $moduleManager->setEvent($event);

        $this->assertSame($event, $moduleManager->getEvent());
        $this->assertSame($moduleManager, $event->getTarget());
    }

    public function testGetEventWillLazyLoadOneWithTargetSetToModuleManager()
    {
        $moduleManager = new ModuleManager([]);
        $event = $moduleManager->getEvent();
        $this->assertInstanceOf(ModuleEvent::class, $event);
        $this->assertSame($moduleManager, $event->getTarget());
    }
}

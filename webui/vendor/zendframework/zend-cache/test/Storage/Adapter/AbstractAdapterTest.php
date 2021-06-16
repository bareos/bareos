<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Cache;
use Zend\Cache\Storage\Adapter\AdapterOptions;
use Zend\Cache\Storage\Plugin\PluginOptions;
use Zend\Cache\Storage\Capabilities;
use Zend\Cache\Exception;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Adapter\AdapterOptions<extended>
 */
class AbstractAdapterTest extends TestCase
{
    // @codingStandardsIgnoreStart
    /**
     * Mock of the abstract storage adapter
     *
     * @var \Zend\Cache\Storage\Adapter\AbstractAdapter
     */
    protected $_storage;

    /**
     * Adapter options
     *
     * @var null|AdapterOptions
     */
    protected $_options;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_options = new AdapterOptions();
    }

    public function testGetOptions()
    {
        $this->_storage = $this->getMockForAbstractAdapter();

        $options = $this->_storage->getOptions();
        $this->assertInstanceOf('Zend\Cache\Storage\Adapter\AdapterOptions', $options);
        $this->assertInternalType('boolean', $options->getWritable());
        $this->assertInternalType('boolean', $options->getReadable());
        $this->assertInternalType('integer', $options->getTtl());
        $this->assertInternalType('string', $options->getNamespace());
        $this->assertInternalType('string', $options->getKeyPattern());
    }

    public function testSetWritable()
    {
        $this->_options->setWritable(true);
        $this->assertTrue($this->_options->getWritable());

        $this->_options->setWritable(false);
        $this->assertFalse($this->_options->getWritable());
    }

    public function testSetReadable()
    {
        $this->_options->setReadable(true);
        $this->assertTrue($this->_options->getReadable());

        $this->_options->setReadable(false);
        $this->assertFalse($this->_options->getReadable());
    }

    public function testSetTtl()
    {
        $this->_options->setTtl('123');
        $this->assertSame(123, $this->_options->getTtl());
    }

    public function testSetTtlThrowsInvalidArgumentException()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setTtl(-1);
    }

    public function testGetDefaultNamespaceNotEmpty()
    {
        $ns = $this->_options->getNamespace();
        $this->assertNotEmpty($ns);
    }

    public function testSetNamespace()
    {
        $this->_options->setNamespace('new_namespace');
        $this->assertSame('new_namespace', $this->_options->getNamespace());
    }

    public function testSetNamespace0()
    {
        $this->_options->setNamespace('0');
        $this->assertSame('0', $this->_options->getNamespace());
    }

    public function testSetKeyPattern()
    {
        $this->_options->setKeyPattern('/^[key]+$/Di');
        $this->assertEquals('/^[key]+$/Di', $this->_options->getKeyPattern());
    }

    public function testUnsetKeyPattern()
    {
        $this->_options->setKeyPattern(null);
        $this->assertSame('', $this->_options->getKeyPattern());
    }

    public function testSetKeyPatternThrowsExceptionOnInvalidPattern()
    {
        $this->expectException('Zend\Cache\Exception\InvalidArgumentException');
        $this->_options->setKeyPattern('#');
    }

    public function testPluginRegistry()
    {
        $this->_storage = $this->getMockForAbstractAdapter();

        $plugin = new \ZendTest\Cache\Storage\TestAsset\MockPlugin();

        // no plugin registered
        $this->assertFalse($this->_storage->hasPlugin($plugin));
        $this->assertEquals(0, count($this->_storage->getPluginRegistry()));
        $this->assertEquals(0, count($plugin->getHandles()));

        // register a plugin
        $this->assertSame($this->_storage, $this->_storage->addPlugin($plugin));
        $this->assertTrue($this->_storage->hasPlugin($plugin));
        $this->assertEquals(1, count($this->_storage->getPluginRegistry()));

        // test registered callback handles
        $handles = $plugin->getHandles();
        $this->assertCount(2, $handles);

        // test unregister a plugin
        $this->assertSame($this->_storage, $this->_storage->removePlugin($plugin));
        $this->assertFalse($this->_storage->hasPlugin($plugin));
        $this->assertEquals(0, count($this->_storage->getPluginRegistry()));
        $this->assertEquals(0, count($plugin->getHandles()));
    }

    public function testInternalTriggerPre()
    {
        $this->_storage = $this->getMockForAbstractAdapter();

        $plugin = new \ZendTest\Cache\Storage\TestAsset\MockPlugin();
        $this->_storage->addPlugin($plugin);

        $params = new \ArrayObject([
            'key'   => 'key1',
            'value' => 'value1'
        ]);

        // call protected method
        $method = new \ReflectionMethod(get_class($this->_storage), 'triggerPre');
        $method->setAccessible(true);
        $rsCollection = $method->invoke($this->_storage, 'setItem', $params);
        $this->assertInstanceOf('Zend\EventManager\ResponseCollection', $rsCollection);

        // test called event
        $calledEvents = $plugin->getCalledEvents();
        $this->assertEquals(1, count($calledEvents));

        $event = current($calledEvents);
        $this->assertInstanceOf('Zend\Cache\Storage\Event', $event);
        $this->assertEquals('setItem.pre', $event->getName());
        $this->assertSame($this->_storage, $event->getTarget());
        $this->assertSame($params, $event->getParams());
    }

    public function testInternalTriggerPost()
    {
        $this->_storage = $this->getMockForAbstractAdapter();

        $plugin = new \ZendTest\Cache\Storage\TestAsset\MockPlugin();
        $this->_storage->addPlugin($plugin);

        $params = new \ArrayObject([
            'key'   => 'key1',
            'value' => 'value1'
        ]);
        $result = true;

        // call protected method
        $method = new \ReflectionMethod(get_class($this->_storage), 'triggerPost');
        $method->setAccessible(true);
        $result = $method->invokeArgs($this->_storage, ['setItem', $params, &$result]);

        // test called event
        $calledEvents = $plugin->getCalledEvents();
        $this->assertEquals(1, count($calledEvents));
        $event = current($calledEvents);

        // return value of triggerPost and the called event should be the same
        $this->assertSame($result, $event->getResult());

        $this->assertInstanceOf('Zend\Cache\Storage\PostEvent', $event);
        $this->assertEquals('setItem.post', $event->getName());
        $this->assertSame($this->_storage, $event->getTarget());
        $this->assertSame($params, $event->getParams());
        $this->assertSame($result, $event->getResult());
    }

    public function testInternalTriggerExceptionThrowRuntimeException()
    {
        $this->_storage = $this->getMockForAbstractAdapter();

        $plugin = new \ZendTest\Cache\Storage\TestAsset\MockPlugin();
        $this->_storage->addPlugin($plugin);

        $result = null;
        $params = new \ArrayObject([
            'key'   => 'key1',
            'value' => 'value1'
        ]);

        // call protected method
        $method = new \ReflectionMethod(get_class($this->_storage), 'triggerException');
        $method->setAccessible(true);

        $this->expectException('Zend\Cache\Exception\RuntimeException');
        $this->expectExceptionMessage('test');
        $method->invokeArgs($this->_storage, ['setItem', $params, & $result, new Exception\RuntimeException('test')]);
    }

    public function testGetItemCallsInternalGetItem()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalGetItem']);

        $key    = 'key1';
        $result = 'value1';

        $this->_storage
            ->expects($this->once())
            ->method('internalGetItem')
            ->with($this->equalTo($key))
            ->will($this->returnValue($result));

        $rs = $this->_storage->getItem($key);
        $this->assertEquals($result, $rs);
    }

    public function testGetItemsCallsInternalGetItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalGetItems']);

        $keys   = ['key1', 'key2'];
        $result = ['key2' => 'value2'];

        $this->_storage
            ->expects($this->once())
            ->method('internalGetItems')
            ->with($this->equalTo($keys))
            ->will($this->returnValue($result));

        $rs = $this->_storage->getItems($keys);
        $this->assertEquals($result, $rs);
    }

    public function testInternalGetItemsCallsInternalGetItemForEachKey()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalGetItem']);

        $items  = ['key1' => 'value1', 'keyNotFound' => false, 'key2' => 'value2'];
        $result = ['key1' => 'value1', 'key2' => 'value2'];

        $i = 0; // method call counter
        foreach ($items as $k => $v) {
            $this->_storage->expects($this->at($i++))
                ->method('internalGetItem')
                ->with(
                    $this->stringContains('key'),
                    $this->anything()
                )
                ->will($this->returnCallback(function ($k, & $success) use ($items) {
                    if ($items[$k]) {
                        $success = true;
                        return $items[$k];
                    } else {
                        $success = false;
                        return;
                    }
                }));
        }

        $rs = $this->_storage->getItems(array_keys($items));
        $this->assertEquals($result, $rs);
    }

    public function testHasItemCallsInternalHasItem()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem']);

        $key    = 'key1';
        $result = true;

        $this->_storage
            ->expects($this->once())
            ->method('internalHasItem')
            ->with($this->equalTo($key))
            ->will($this->returnValue($result));

        $rs = $this->_storage->hasItem($key);
        $this->assertSame($result, $rs);
    }

    public function testHasItemsCallsInternalHasItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItems']);

        $keys   = ['key1', 'key2'];
        $result = ['key2'];

        $this->_storage
            ->expects($this->once())
            ->method('internalHasItems')
            ->with($this->equalTo($keys))
            ->will($this->returnValue($result));

        $rs = $this->_storage->hasItems($keys);
        $this->assertEquals($result, $rs);
    }

    public function testInternalHasItemsCallsInternalHasItem()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem']);

        $items  = ['key1' => true];

        $this->_storage
            ->expects($this->atLeastOnce())
            ->method('internalHasItem')
            ->with($this->equalTo('key1'))
            ->will($this->returnValue(true));

        $rs = $this->_storage->hasItems(array_keys($items));
        $this->assertEquals(['key1'], $rs);
    }

    public function testGetItemReturnsNullIfFailed()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalGetItem']);

        $key    = 'key1';

        // Do not throw exceptions outside the adapter
        $pluginOptions = new PluginOptions(
            ['throw_exceptions' => false]
        );
        $plugin = new Cache\Storage\Plugin\ExceptionHandler();
        $plugin->setOptions($pluginOptions);
        $this->_storage->addPlugin($plugin);

        // Simulate internalGetItem() throwing an exception
        $this->_storage
            ->expects($this->once())
            ->method('internalGetItem')
            ->with($this->equalTo($key))
            ->will($this->throwException(new \Exception('internalGetItem failed')));

        $result = $this->_storage->getItem($key, $success);
        $this->assertNull($result, 'GetItem should return null the item cannot be retrieved');
        $this->assertFalse($success, '$success should be false if the item cannot be retrieved');
    }

    public function simpleEventHandlingMethodDefinitions()
    {
        $capabilities = new Capabilities($this->getMockForAbstractAdapter(), new \stdClass);

        return [
            //    name, internalName, args, returnValue
            ['hasItem', 'internalGetItem', ['k'], 'v'],
            ['hasItems', 'internalHasItems', [['k1', 'k2']], ['v1', 'v2']],

            ['getItem', 'internalGetItem', ['k'], 'v'],
            ['getItems', 'internalGetItems', [['k1', 'k2']], ['k1' => 'v1', 'k2' => 'v2']],

            ['getMetadata', 'internalGetMetadata', ['k'], []],
            ['getMetadatas', 'internalGetMetadatas', [['k1', 'k2']], ['k1' => [], 'k2' => []]],

            ['setItem', 'internalSetItem', ['k', 'v'], true],
            ['setItems', 'internalSetItems', [['k1' => 'v1', 'k2' => 'v2']], []],

            ['replaceItem', 'internalReplaceItem', ['k', 'v'], true],
            ['replaceItems', 'internalReplaceItems', [['k1' => 'v1', 'k2' => 'v2']], []],

            ['addItem', 'internalAddItem', ['k', 'v'], true],
            ['addItems', 'internalAddItems', [['k1' => 'v1', 'k2' => 'v2']], []],

            ['checkAndSetItem', 'internalCheckAndSetItem', [123, 'k', 'v'], true],

            ['touchItem', 'internalTouchItem', ['k'], true],
            ['touchItems', 'internalTouchItems', [['k1', 'k2']], []],

            ['removeItem', 'internalRemoveItem', ['k'], true],
            ['removeItems', 'internalRemoveItems', [['k1', 'k2']], []],

            ['incrementItem', 'internalIncrementItem', ['k', 1], true],
            ['incrementItems', 'internalIncrementItems', [['k1' => 1, 'k2' => 2]], []],

            ['decrementItem', 'internalDecrementItem', ['k', 1], true],
            ['decrementItems', 'internalDecrementItems', [['k1' => 1, 'k2' => 2]], []],

            ['getCapabilities', 'internalGetCapabilities', [], $capabilities],
        ];
    }

    /**
     * @dataProvider simpleEventHandlingMethodDefinitions
     */
    public function testEventHandlingSimple($methodName, $internalMethodName, $methodArgs, $retVal)
    {
        $this->_storage = $this->getMockForAbstractAdapter([$internalMethodName]);

        $eventList    = [];
        $eventHandler = function ($event) use (&$eventList) {
            $eventList[] = $event->getName();
        };
        $this->_storage->getEventManager()->attach($methodName . '.pre', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.post', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.exception', $eventHandler);

        $mock = $this->_storage
            ->expects($this->once())
            ->method($internalMethodName);
        $mock = call_user_func_array([$mock, 'with'], array_map([$this, 'equalTo'], $methodArgs));
        $mock->will($this->returnValue($retVal));

        call_user_func_array([$this->_storage, $methodName], $methodArgs);

        $expectedEventList = [
            $methodName . '.pre',
            $methodName . '.post'
        ];
        $this->assertSame($expectedEventList, $eventList);
    }

    /**
     * @dataProvider simpleEventHandlingMethodDefinitions
     */
    public function testEventHandlingCatchException($methodName, $internalMethodName, $methodArgs, $retVal)
    {
        $this->_storage = $this->getMockForAbstractAdapter([$internalMethodName]);

        $eventList    = [];
        $eventHandler = function ($event) use (&$eventList) {
            $eventList[] = $event->getName();
            if ($event instanceof Cache\Storage\ExceptionEvent) {
                $event->setThrowException(false);
            }
        };
        $this->_storage->getEventManager()->attach($methodName . '.pre', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.post', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.exception', $eventHandler);

        $mock = $this->_storage
            ->expects($this->once())
            ->method($internalMethodName);
        $mock = call_user_func_array([$mock, 'with'], array_map([$this, 'equalTo'], $methodArgs));
        $mock->will($this->throwException(new \Exception('test')));

        call_user_func_array([$this->_storage, $methodName], $methodArgs);

        $expectedEventList = [
            $methodName . '.pre',
            $methodName . '.exception',
        ];
        $this->assertSame($expectedEventList, $eventList);
    }

    /**
     * @dataProvider simpleEventHandlingMethodDefinitions
     */
    public function testEventHandlingStopInPre($methodName, $internalMethodName, $methodArgs, $retVal)
    {
        $this->_storage = $this->getMockForAbstractAdapter([$internalMethodName]);

        $eventList    = [];
        $eventHandler = function ($event) use (&$eventList) {
            $eventList[] = $event->getName();
        };
        $this->_storage->getEventManager()->attach($methodName . '.pre', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.post', $eventHandler);
        $this->_storage->getEventManager()->attach($methodName . '.exception', $eventHandler);

        $this->_storage->getEventManager()->attach($methodName . '.pre', function ($event) use ($retVal) {
            $event->stopPropagation();
            return $retVal;
        });

        // the internal method should never be called
        $this->_storage->expects($this->never())->method($internalMethodName);

        // the return vaue should be available by pre-event
        $result = call_user_func_array([$this->_storage, $methodName], $methodArgs);
        $this->assertSame($retVal, $result);

        // after the triggered pre-event the post-event should be triggered as well
        $expectedEventList = [
            $methodName . '.pre',
            $methodName . '.post',
        ];
        $this->assertSame($expectedEventList, $eventList);
    }

    public function testGetMetadatas()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['getMetadata', 'internalGetMetadata']);

        $meta  = ['meta' => 'data'];
        $items = [
            'key1' => $meta,
            'key2' => $meta,
        ];

        // foreach item call 'internalGetMetadata' instead of 'getMetadata'
        $this->_storage->expects($this->never())->method('getMetadata');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalGetMetadata')
            ->with($this->stringContains('key'))
            ->will($this->returnValue($meta));

        $this->assertSame($items, $this->_storage->getMetadatas(array_keys($items)));
    }

    public function testGetMetadatasFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalGetMetadata']);

        $items = ['key1', 'key2'];

        // return false to indicate that the operation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalGetMetadata')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(false));

        $this->assertSame([], $this->_storage->getMetadatas($items));
    }

    public function testSetItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['setItem', 'internalSetItem']);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2',
        ];

        // foreach item call 'internalSetItem' instead of 'setItem'
        $this->_storage->expects($this->never())->method('setItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(true));

        $this->assertSame([], $this->_storage->setItems($items));
    }

    public function testSetItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalSetItem']);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2',
        ];

        // return false to indicate that the operation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(false));

        $this->assertSame(array_keys($items), $this->_storage->setItems($items));
    }

    public function testAddItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter([
            'getItem', 'internalGetItem',
            'hasItem', 'internalHasItem',
            'setItem', 'internalSetItem'
        ]);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2'
        ];

        // first check if the items already exists using has
        // call 'internalHasItem' instead of 'hasItem' or '[internal]GetItem'
        $this->_storage->expects($this->never())->method('hasItem');
        $this->_storage->expects($this->never())->method('getItem');
        $this->_storage->expects($this->never())->method('internalGetItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(false));

        // If not create the items using set
        // call 'internalSetItem' instead of 'setItem'
        $this->_storage->expects($this->never())->method('setItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(true));

        $this->assertSame([], $this->_storage->addItems($items));
    }

    public function testAddItemsExists()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem', 'internalSetItem']);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2',
            'key3' => 'value3',
        ];

        // first check if items already exists
        // -> return true to indicate that the item already exist
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(true));

        // set item should never be called
        $this->_storage->expects($this->never())->method('internalSetItem');

        $this->assertSame(array_keys($items), $this->_storage->addItems($items));
    }

    public function testAddItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem', 'internalSetItem']);

        $items   = [
            'key1' => 'value1',
            'key2' => 'value2',
            'key3' => 'value3',
        ];

        // first check if items already exists
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(false));

        // if not create the items
        // -> return false to indicate creation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(false));

        $this->assertSame(array_keys($items), $this->_storage->addItems($items));
    }

    public function testReplaceItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter([
            'hasItem', 'internalHasItem',
            'getItem', 'internalGetItem',
            'setItem', 'internalSetItem',
        ]);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2'
        ];

        // First check if the item already exists using has
        // call 'internalHasItem' instead of 'hasItem' or '[internal]GetItem'
        $this->_storage->expects($this->never())->method('hasItem');
        $this->_storage->expects($this->never())->method('getItem');
        $this->_storage->expects($this->never())->method('internalGetItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(true));

        // if yes overwrite the items
        // call 'internalSetItem' instead of 'setItem'
        $this->_storage->expects($this->never())->method('setItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(true));

        $this->assertSame([], $this->_storage->replaceItems($items));
    }

    public function testReplaceItemsMissing()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem', 'internalSetItem']);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2',
            'key3' => 'value3',
        ];

        // First check if the items already exists
        // -> return false to indicate the items doesn't exists
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(false));

        // writing items should never be called
        $this->_storage->expects($this->never())->method('internalSetItem');

        $this->assertSame(array_keys($items), $this->_storage->replaceItems($items));
    }

    public function testReplaceItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalHasItem', 'internalSetItem']);

        $items = [
            'key1' => 'value1',
            'key2' => 'value2',
            'key3' => 'value3',
        ];

        // First check if the items already exists
        // -> return true to indicate the items exists
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalHasItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(true));

        // if yes overwrite the items
        // -> return false to indicate that overwriting failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalSetItem')
            ->with($this->stringContains('key'), $this->stringContains('value'))
            ->will($this->returnValue(false));

        $this->assertSame(array_keys($items), $this->_storage->replaceItems($items));
    }

    public function testRemoveItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['removeItem', 'internalRemoveItem']);

        $keys = ['key1', 'key2'];

        // call 'internalRemoveItem' instaed of 'removeItem'
        $this->_storage->expects($this->never())->method('removeItem');
        $this->_storage->expects($this->exactly(count($keys)))
            ->method('internalRemoveItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(true));

        $this->assertSame([], $this->_storage->removeItems($keys));
    }

    public function testRemoveItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalRemoveItem']);

        $keys = ['key1', 'key2', 'key3'];

        // call 'internalRemoveItem'
        // -> return false to indicate that no item was removed
        $this->_storage->expects($this->exactly(count($keys)))
                       ->method('internalRemoveItem')
                       ->with($this->stringContains('key'))
                       ->will($this->returnValue(false));

        $this->assertSame($keys, $this->_storage->removeItems($keys));
    }

    public function testIncrementItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['incrementItem', 'internalIncrementItem']);

        $items = [
            'key1' => 2,
            'key2' => 2,
        ];

        // foreach item call 'internalIncrementItem' instead of 'incrementItem'
        $this->_storage->expects($this->never())->method('incrementItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalIncrementItem')
            ->with($this->stringContains('key'), $this->equalTo(2))
            ->will($this->returnValue(4));

        $this->assertSame([
            'key1' => 4,
            'key2' => 4,
        ], $this->_storage->incrementItems($items));
    }

    public function testIncrementItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalIncrementItem']);

        $items = [
            'key1' => 2,
            'key2' => 2,
        ];

        // return false to indicate that the operation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalIncrementItem')
            ->with($this->stringContains('key'), $this->equalTo(2))
            ->will($this->returnValue(false));

        $this->assertSame([], $this->_storage->incrementItems($items));
    }

    public function testDecrementItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['decrementItem', 'internalDecrementItem']);

        $items = [
            'key1' => 2,
            'key2' => 2,
        ];

        // foreach item call 'internalDecrementItem' instead of 'decrementItem'
        $this->_storage->expects($this->never())->method('decrementItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalDecrementItem')
            ->with($this->stringContains('key'), $this->equalTo(2))
            ->will($this->returnValue(4));

        $this->assertSame([
            'key1' => 4,
            'key2' => 4,
        ], $this->_storage->decrementItems($items));
    }

    public function testDecrementItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalDecrementItem']);

        $items = [
            'key1' => 2,
            'key2' => 2,
        ];

        // return false to indicate that the operation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalDecrementItem')
            ->with($this->stringContains('key'), $this->equalTo(2))
            ->will($this->returnValue(false));

        $this->assertSame([], $this->_storage->decrementItems($items));
    }

    public function testTouchItems()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['touchItem', 'internalTouchItem']);

        $items = ['key1', 'key2'];

        // foreach item call 'internalTouchItem' instead of 'touchItem'
        $this->_storage->expects($this->never())->method('touchItem');
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalTouchItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(true));

        $this->assertSame([], $this->_storage->touchItems($items));
    }

    public function testTouchItemsFail()
    {
        $this->_storage = $this->getMockForAbstractAdapter(['internalTouchItem']);

        $items = ['key1', 'key2'];

        // return false to indicate that the operation failed
        $this->_storage->expects($this->exactly(count($items)))
            ->method('internalTouchItem')
            ->with($this->stringContains('key'))
            ->will($this->returnValue(false));

        $this->assertSame($items, $this->_storage->touchItems($items));
    }

    public function testPreEventsCanChangeArguments()
    {
        // getItem(s)
        $this->checkPreEventCanChangeArguments('getItem', [
            'key' => 'key'
        ], [
            'key' => 'changedKey',
        ]);

        $this->checkPreEventCanChangeArguments('getItems', [
            'keys' => ['key']
        ], [
            'keys' => ['changedKey'],
        ]);

        // hasItem(s)
        $this->checkPreEventCanChangeArguments('hasItem', [
            'key' => 'key'
        ], [
            'key' => 'changedKey',
        ]);

        $this->checkPreEventCanChangeArguments('hasItems', [
            'keys' => ['key'],
        ], [
            'keys' => ['changedKey'],
        ]);

        // getMetadata(s)
        $this->checkPreEventCanChangeArguments('getMetadata', [
            'key' => 'key'
        ], [
            'key' => 'changedKey',
        ]);

        $this->checkPreEventCanChangeArguments('getMetadatas', [
            'keys' => ['key'],
        ], [
            'keys' => ['changedKey'],
        ]);

        // setItem(s)
        $this->checkPreEventCanChangeArguments('setItem', [
            'key'   => 'key',
            'value' => 'value',
        ], [
            'key'   => 'changedKey',
            'value' => 'changedValue',
        ]);

        $this->checkPreEventCanChangeArguments('setItems', [
            'keyValuePairs' => ['key' => 'value'],
        ], [
            'keyValuePairs' => ['changedKey' => 'changedValue'],
        ]);

        // addItem(s)
        $this->checkPreEventCanChangeArguments('addItem', [
            'key'   => 'key',
            'value' => 'value',
        ], [
            'key'   => 'changedKey',
            'value' => 'changedValue',
        ]);

        $this->checkPreEventCanChangeArguments('addItems', [
            'keyValuePairs' => ['key' => 'value'],
        ], [
            'keyValuePairs' => ['changedKey' => 'changedValue'],
        ]);

        // replaceItem(s)
        $this->checkPreEventCanChangeArguments('replaceItem', [
            'key'   => 'key',
            'value' => 'value',
        ], [
            'key'   => 'changedKey',
            'value' => 'changedValue',
        ]);

        $this->checkPreEventCanChangeArguments('replaceItems', [
            'keyValuePairs' => ['key' => 'value'],
        ], [
            'keyValuePairs' => ['changedKey' => 'changedValue'],
        ]);

        // CAS
        $this->checkPreEventCanChangeArguments('checkAndSetItem', [
            'token' => 'token',
            'key'   => 'key',
            'value' => 'value',
        ], [
            'token' => 'changedToken',
            'key'   => 'changedKey',
            'value' => 'changedValue',
        ]);

        // touchItem(s)
        $this->checkPreEventCanChangeArguments('touchItem', [
            'key' => 'key',
        ], [
            'key' => 'changedKey',
        ]);

        $this->checkPreEventCanChangeArguments('touchItems', [
            'keys' => ['key'],
        ], [
            'keys' => ['changedKey'],
        ]);

        // removeItem(s)
        $this->checkPreEventCanChangeArguments('removeItem', [
            'key' => 'key',
        ], [
            'key' => 'changedKey',
        ]);

        $this->checkPreEventCanChangeArguments('removeItems', [
            'keys' => ['key'],
        ], [
            'keys' => ['changedKey'],
        ]);

        // incrementItem(s)
        $this->checkPreEventCanChangeArguments('incrementItem', [
            'key'   => 'key',
            'value' => 1
        ], [
            'key'   => 'changedKey',
            'value' => 2,
        ]);

        $this->checkPreEventCanChangeArguments('incrementItems', [
            'keyValuePairs' => ['key' => 1],
        ], [
            'keyValuePairs' => ['changedKey' => 2],
        ]);

        // decrementItem(s)
        $this->checkPreEventCanChangeArguments('decrementItem', [
            'key'   => 'key',
            'value' => 1
        ], [
            'key'   => 'changedKey',
            'value' => 2,
        ]);

        $this->checkPreEventCanChangeArguments('decrementItems', [
            'keyValuePairs' => ['key' => 1],
        ], [
            'keyValuePairs' => ['changedKey' => 2],
        ]);
    }

    protected function checkPreEventCanChangeArguments($method, array $args, array $expectedArgs)
    {
        $internalMethod = 'internal' . ucfirst($method);
        $eventName      = $method . '.pre';

        // init mock
        $this->_storage = $this->getMockForAbstractAdapter([$internalMethod]);
        $this->_storage->getEventManager()->attach($eventName, function ($event) use ($expectedArgs) {
            $params = $event->getParams();
            foreach ($expectedArgs as $k => $v) {
                $params[$k] = $v;
            }
        });

        // set expected arguments of internal method call
        $tmp = $this->_storage->expects($this->once())->method($internalMethod);
        $equals = [];
        foreach ($expectedArgs as $v) {
            $equals[] = $this->equalTo($v);
        }
        call_user_func_array([$tmp, 'with'], $equals);

        // run
        call_user_func_array([$this->_storage, $method], $args);
    }

    /**
     * Generates a mock of the abstract storage adapter by mocking all abstract and the given methods
     * Also sets the adapter options
     *
     * @param array $methods
     * @return \Zend\Cache\Storage\Adapter\AbstractAdapter
     */
    protected function getMockForAbstractAdapter(array $methods = [])
    {
        $class = 'Zend\Cache\Storage\Adapter\AbstractAdapter';

        if (! $methods) {
            $adapter = $this->getMockForAbstractClass($class);
        } else {
            $reflection = new \ReflectionClass('Zend\Cache\Storage\Adapter\AbstractAdapter');
            foreach ($reflection->getMethods() as $method) {
                if ($method->isAbstract()) {
                    $methods[] = $method->getName();
                }
            }
            $adapter = $this->getMockBuilder($class)
                ->setMethods(array_unique($methods))
                ->disableArgumentCloning()
                ->getMock();
        }

        $this->_options = $this->_options ?: new AdapterOptions();
        $adapter->setOptions($this->_options);
        return $adapter;
    }
}

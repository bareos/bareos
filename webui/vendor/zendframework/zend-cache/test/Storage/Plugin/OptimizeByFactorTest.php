<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\Plugin;

use ArrayObject;
use Zend\Cache;
use Zend\Cache\Storage\PostEvent;
use Zend\EventManager\Test\EventListenerIntrospectionTrait;
use ZendTest\Cache\Storage\TestAsset\OptimizableMockAdapter;

/**
 * @covers Zend\Cache\Storage\Plugin\OptimizeByFactor<extended>
 */
class OptimizeByFactorTest extends CommonPluginTest
{
    use EventListenerIntrospectionTrait;

    // @codingStandardsIgnoreStart
    /**
     * The storage adapter
     *
     * @var \Zend\Cache\Storage\Adapter\AbstractAdapter
     */
    protected $_adapter;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_adapter = new OptimizableMockAdapter();
        $this->_options = new Cache\Storage\Plugin\PluginOptions([
            'optimizing_factor' => 1,
        ]);
        $this->_plugin  = new Cache\Storage\Plugin\OptimizeByFactor();
        $this->_plugin->setOptions($this->_options);
    }

    public function getCommonPluginNamesProvider()
    {
        return [
            ['optimize_by_factor'],
            ['optimizebyfactor'],
            ['OptimizeByFactor'],
            ['optimizeByFactor'],
        ];
    }

    public function testAddPlugin()
    {
        $this->_adapter->addPlugin($this->_plugin);

        // check attached callbacks
        $expectedListeners = [
            'removeItem.post'  => 'optimizeByFactor',
            'removeItems.post' => 'optimizeByFactor',
        ];
        foreach ($expectedListeners as $eventName => $expectedCallbackMethod) {
            $listeners = $this->getArrayOfListenersForEvent($eventName, $this->_adapter->getEventManager());

            // event should attached only once
            $this->assertSame(1, count($listeners));

            // check expected callback method
            $cb = array_shift($listeners);
            $this->assertArrayHasKey(0, $cb);
            $this->assertSame($this->_plugin, $cb[0]);
            $this->assertArrayHasKey(1, $cb);
            $this->assertSame($expectedCallbackMethod, $cb[1]);
        }
    }

    public function testRemovePlugin()
    {
        $this->_adapter->addPlugin($this->_plugin);
        $this->_adapter->removePlugin($this->_plugin);

        // no events should be attached
        $this->assertEquals(0, count($this->getEventsFromEventManager($this->_adapter->getEventManager())));
    }

    public function testOptimizeByFactor()
    {
        $adapter = $this->getMockBuilder(get_class($this->_adapter))
            ->setMethods(['optimize'])
            ->getMock();

        // test optimize will be called
        $adapter
            ->expects($this->once())
            ->method('optimize');

        // call event callback
        $result = true;
        $event = new PostEvent('removeItem.post', $adapter, new ArrayObject([
            'options' => []
        ]), $result);

        $this->_plugin->optimizeByFactor($event);

        $this->assertTrue($event->getResult());
    }
}

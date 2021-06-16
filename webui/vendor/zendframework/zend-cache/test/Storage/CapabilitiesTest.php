<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\Capabilities;
use Zend\Cache\Storage\Adapter\Memory as MemoryAdapter;

/**
 * @group      Zend_Cache
 * @covers Zend\Cache\Storage\Capabilities
 */
class CapabilitiesTest extends TestCase
{
    // @codingStandardsIgnoreStart
    /**
     * Capabilities instance
     *
     * @var Capabilities
     */
    protected $_capabilities;

    /**
     * Base capabilities instance
     *
     * @var Capabilities
     */
    protected $_baseCapabilities;

    /**
     * Set/Change marker
     *
     * @var \stdClass
     */
    protected $_marker;

    /**
     * The storage adapter
     *
     * @var MemoryAdapter
     */
    protected $_adapter;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        $this->_marker  = new \stdClass();
        $this->_adapter = new MemoryAdapter();

        $this->_baseCapabilities = new Capabilities($this->_adapter, $this->_marker);
        $this->_capabilities     = new Capabilities($this->_adapter, $this->_marker, [], $this->_baseCapabilities);
    }

    public function testGetAdapter()
    {
        $this->assertSame($this->_adapter, $this->_capabilities->getAdapter());
        $this->assertSame($this->_adapter, $this->_baseCapabilities->getAdapter());
    }

    public function testSetAndGetCapability()
    {
        $this->_capabilities->setMaxTtl($this->_marker, 100);
        $this->assertEquals(100, $this->_capabilities->getMaxTtl());
    }

    public function testGetCapabilityByBaseCapabilities()
    {
        $this->_baseCapabilities->setMaxTtl($this->_marker, 100);
        $this->assertEquals(100, $this->_capabilities->getMaxTtl());
    }

    public function testTriggerCapabilityEvent()
    {
        $em    = $this->_capabilities->getAdapter()->getEventManager();
        $event = null;
        $em->attach('capability', function ($eventArg) use (&$event) {
            $event = $eventArg;
        });

        $this->_capabilities->setMaxTtl($this->_marker, 100);

        $this->assertInstanceOf('Zend\EventManager\Event', $event);
        $this->assertEquals('capability', $event->getName());
        $this->assertSame($this->_adapter, $event->getTarget());

        $params = $event->getParams();
        $this->assertInstanceOf('ArrayObject', $params);
        $this->assertTrue(isset($params ['maxTtl']));
        $this->assertEquals(100, $params['maxTtl']);
    }
}

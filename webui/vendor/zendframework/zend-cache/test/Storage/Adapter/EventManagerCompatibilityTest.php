<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 * @package   Zend_Cache
 */

namespace ZendTest\Cache\Storage\Adapter;

use PHPUnit\Framework\TestCase;
use Zend\Cache\Storage\Adapter\AbstractAdapter;
use Zend\EventManager\EventManager;
use ZendTest\Cache\Storage\TestAsset\MockAdapter;

class EventManagerCompatibilityTest extends TestCase
{
    public function setUp()
    {
        $this->adapter = new MockAdapter();
    }

    public function testCanLazyLoadEventManager()
    {
        $events = $this->adapter->getEventManager();
        $this->assertInstanceOf(EventManager::class, $events);
        return $events;
    }

    /**
     * @depends testCanLazyLoadEventManager
     */
    public function testLazyLoadedEventManagerIsInjectedProperlyWithDefaultIdentifiers(EventManager $events)
    {
        $this->assertEquals([
            AbstractAdapter::class,
            MockAdapter::class,
        ], $events->getIdentifiers());
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-cache for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-cache/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Cache\Psr\CacheItemPool;

use DateInterval;
use DateTime;
use PHPUnit\Framework\TestCase;
use Zend\Cache\Psr\CacheItemPool\CacheItem;

class CacheItemTest extends TestCase
{
    private $tz;

    protected function setUp()
    {
        // set non-UTC timezone
        $this->tz = date_default_timezone_get();
        date_default_timezone_set('America/Vancouver');
    }

    protected function tearDown()
    {
        date_default_timezone_set($this->tz);
    }

    public function testConstructorIsHit()
    {
        $item = new CacheItem('key', 'value', true);
        $this->assertEquals('key', $item->getKey());
        $this->assertEquals('value', $item->get());
        $this->assertTrue($item->isHit());
    }

    public function testConstructorIsNotHit()
    {
        $item = new CacheItem('key', 'value', false);
        $this->assertEquals('key', $item->getKey());
        $this->assertNull($item->get());
        $this->assertFalse($item->isHit());
    }

    public function testSet()
    {
        $item = new CacheItem('key', 'value', true);
        $return = $item->set('value2');
        $this->assertEquals($item, $return);
        $this->assertEquals('value2', $item->get());
    }

    public function testExpiresAtDateTime()
    {
        $item = new CacheItem('key', 'value', true);
        $dateTime = new DateTime('+5 seconds');
        $return = $item->expiresAt($dateTime);
        $this->assertEquals($item, $return);
        $this->assertEquals(5, $item->getTtl());
    }

    public function testExpireAtNull()
    {
        $item = new CacheItem('key', 'value', true);
        $return = $item->expiresAt(null);
        $this->assertEquals($item, $return);
        $this->assertNull($item->getTtl());
    }

    /**
     * @expectedException \Zend\Cache\Psr\CacheItemPool\InvalidArgumentException
     */
    public function testExpireAtInvalidThrowsException()
    {
        $item = new CacheItem('key', 'value', true);
        $item->expiresAt('foo');
    }

    public function testExpiresAfterInt()
    {
        $item = new CacheItem('key', 'value', true);
        $return = $item->expiresAfter(3600);
        $this->assertEquals($item, $return);
        $this->assertEquals(3600, $item->getTtl());
    }

    public function testExpiresAfterInterval()
    {
        $item = new CacheItem('key', 'value', true);
        $interval = new DateInterval('PT1H');
        $return = $item->expiresAfter($interval);
        $this->assertEquals($item, $return);
        $this->assertEquals(3600, $item->getTtl());
    }

    public function testExpiresAfterNull()
    {
        $item = new CacheItem('key', 'value', true);
        $item->expiresAfter(null);
        $this->assertNull($item->getTtl());
    }

    /**
     * @expectedException \Zend\Cache\Psr\CacheItemPool\InvalidArgumentException
     */
    public function testExpiresAfterInvalidThrowsException()
    {
        $item = new CacheItem('key', 'value', true);
        $item->expiresAfter([]);
    }
}

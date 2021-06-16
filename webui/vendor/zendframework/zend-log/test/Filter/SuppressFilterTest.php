<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Log\Filter;

use PHPUnit\Framework\TestCase;
use Zend\Log\Filter\SuppressFilter;

class SuppressFilterTest extends TestCase
{
    protected function setUp()
    {
        $this->filter = new SuppressFilter();
    }

    public function testSuppressIsInitiallyOff()
    {
        $this->assertTrue($this->filter->filter([]));
    }

    public function testSuppressByConstructorBoolean()
    {
        $this->filter = new SuppressFilter(true);
        $this->assertFalse($this->filter->filter([]));
        $this->assertFalse($this->filter->filter([]));
    }

    public function testSuppressByConstructorArray()
    {
        $this->filter = new SuppressFilter(['suppress' => true]);
        $this->assertFalse($this->filter->filter([]));
        $this->assertFalse($this->filter->filter([]));
    }

    public function testConstructorThrowsOnInvalidSuppressValue()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Suppress must be a boolean');
        new SuppressFilter('foo');
    }

    public function testSuppressOn()
    {
        $this->filter->suppress(true);
        $this->assertFalse($this->filter->filter([]));
        $this->assertFalse($this->filter->filter([]));
    }

    public function testSuppressOff()
    {
        $this->filter->suppress(false);
        $this->assertTrue($this->filter->filter([]));
        $this->assertTrue($this->filter->filter([]));
    }

    public function testSuppressCanBeReset()
    {
        $this->filter->suppress(true);
        $this->assertFalse($this->filter->filter([]));
        $this->filter->suppress(false);
        $this->assertTrue($this->filter->filter([]));
        $this->filter->suppress(true);
        $this->assertFalse($this->filter->filter([]));
    }
}

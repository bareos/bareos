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
use Zend\Log\Filter\Priority;

/**
 * @group      Zend_Log
 */
class PriorityTest extends TestCase
{
    public function testComparisonDefaultsToLessThanOrEqual()
    {
        // accept at or below priority 2
        $filter = new Priority(2);

        $this->assertTrue($filter->filter(['priority' => 2]));
        $this->assertTrue($filter->filter(['priority' => 1]));
        $this->assertFalse($filter->filter(['priority' => 3]));
    }

    public function testComparisonOperatorCanBeChanged()
    {
        // accept above priority 2
        $filter = new Priority(2, '>');

        $this->assertTrue($filter->filter(['priority' => 3]));
        $this->assertFalse($filter->filter(['priority' => 2]));
        $this->assertFalse($filter->filter(['priority' => 1]));
    }

    public function testConstructorThrowsOnInvalidPriority()
    {
        $this->expectException('Zend\Log\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('must be a number');
        new Priority('foo');
    }

    public function testComparisonStringSupport()
    {
        // accept at or below priority '2'
        $filter = new Priority('2');

        $this->assertTrue($filter->filter(['priority' => 2]));
        $this->assertTrue($filter->filter(['priority' => 1]));
        $this->assertFalse($filter->filter(['priority' => 3]));
    }
}

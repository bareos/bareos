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
use Zend\Log\Filter\Mock as MockFilter;

/**
 * @group      Zend_Log
 */
class MockTest extends TestCase
{
    public function testWrite()
    {
        $filter = new MockFilter();
        $this->assertSame([], $filter->events);

        $fields = ['foo' => 'bar'];
        $this->assertTrue($filter->filter($fields));
        $this->assertSame([$fields], $filter->events);
    }
}

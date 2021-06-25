<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;
use Zend\Http\Header\Range;

class RangeTest extends TestCase
{
    public function testRangeFromStringCreatesValidRangeHeader()
    {
        $rangeHeader = Range::fromString('Range: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $rangeHeader);
        $this->assertInstanceOf(Range::class, $rangeHeader);
    }

    public function testRangeGetFieldNameReturnsHeaderName()
    {
        $rangeHeader = new Range();
        $this->assertEquals('Range', $rangeHeader->getFieldName());
    }

    public function testRangeGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('Range needs to be completed');

        $rangeHeader = new Range();
        $this->assertEquals('xxx', $rangeHeader->getFieldValue());
    }

    public function testRangeToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Range needs to be completed');

        $rangeHeader = new Range();

        // @todo set some values, then test output
        $this->assertEmpty('Range: xxx', $rangeHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Range::fromString("Range: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructorValue()
    {
        $this->expectException(InvalidArgumentException::class);
        new Range("xxx\r\n\r\nevilContent");
    }
}

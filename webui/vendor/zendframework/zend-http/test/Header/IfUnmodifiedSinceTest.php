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
use Zend\Http\Header\IfUnmodifiedSince;

class IfUnmodifiedSinceTest extends TestCase
{
    public function testIfUnmodifiedSinceFromStringCreatesValidIfUnmodifiedSinceHeader()
    {
        $ifUnmodifiedSinceHeader = IfUnmodifiedSince::fromString('If-Unmodified-Since: Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertInstanceOf(HeaderInterface::class, $ifUnmodifiedSinceHeader);
        $this->assertInstanceOf(IfUnmodifiedSince::class, $ifUnmodifiedSinceHeader);
    }

    public function testIfUnmodifiedSinceGetFieldNameReturnsHeaderName()
    {
        $ifUnmodifiedSinceHeader = new IfUnmodifiedSince();
        $this->assertEquals('If-Unmodified-Since', $ifUnmodifiedSinceHeader->getFieldName());
    }

    public function testIfUnmodifiedSinceGetFieldValueReturnsProperValue()
    {
        $ifUnmodifiedSinceHeader = new IfUnmodifiedSince();
        $ifUnmodifiedSinceHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Sun, 06 Nov 1994 08:49:37 GMT', $ifUnmodifiedSinceHeader->getFieldValue());
    }

    public function testIfUnmodifiedSinceToStringReturnsHeaderFormattedString()
    {
        $ifUnmodifiedSinceHeader = new IfUnmodifiedSince();
        $ifUnmodifiedSinceHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('If-Unmodified-Since: Sun, 06 Nov 1994 08:49:37 GMT', $ifUnmodifiedSinceHeader->toString());
    }

    /**
     * Implementation specific tests are covered by DateTest
     * @see ZendTest\Http\Header\DateTest
     */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testCRLFAttack()
    {
        $this->expectException(InvalidArgumentException::class);
        IfUnmodifiedSince::fromString(
            "If-Unmodified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\nevilContent"
        );
    }
}

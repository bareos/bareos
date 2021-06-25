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
use Zend\Http\Header\IfMatch;

class IfMatchTest extends TestCase
{
    public function testIfMatchFromStringCreatesValidIfMatchHeader()
    {
        $ifMatchHeader = IfMatch::fromString('If-Match: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $ifMatchHeader);
        $this->assertInstanceOf(IfMatch::class, $ifMatchHeader);
    }

    public function testIfMatchGetFieldNameReturnsHeaderName()
    {
        $ifMatchHeader = new IfMatch();
        $this->assertEquals('If-Match', $ifMatchHeader->getFieldName());
    }

    public function testIfMatchGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('IfMatch needs to be completed');

        $ifMatchHeader = new IfMatch();
        $this->assertEquals('xxx', $ifMatchHeader->getFieldValue());
    }

    public function testIfMatchToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('IfMatch needs to be completed');

        $ifMatchHeader = new IfMatch();

        // @todo set some values, then test output
        $this->assertEmpty('If-Match: xxx', $ifMatchHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        IfMatch::fromString("If-Match: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new IfMatch("xxx\r\n\r\nevilContent");
    }
}

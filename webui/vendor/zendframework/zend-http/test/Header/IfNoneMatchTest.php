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
use Zend\Http\Header\IfNoneMatch;

class IfNoneMatchTest extends TestCase
{
    public function testIfNoneMatchFromStringCreatesValidIfNoneMatchHeader()
    {
        $ifNoneMatchHeader = IfNoneMatch::fromString('If-None-Match: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $ifNoneMatchHeader);
        $this->assertInstanceOf(IfNoneMatch::class, $ifNoneMatchHeader);
    }

    public function testIfNoneMatchGetFieldNameReturnsHeaderName()
    {
        $ifNoneMatchHeader = new IfNoneMatch();
        $this->assertEquals('If-None-Match', $ifNoneMatchHeader->getFieldName());
    }

    public function testIfNoneMatchGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('IfNoneMatch needs to be completed');

        $ifNoneMatchHeader = new IfNoneMatch();
        $this->assertEquals('xxx', $ifNoneMatchHeader->getFieldValue());
    }

    public function testIfNoneMatchToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('IfNoneMatch needs to be completed');

        $ifNoneMatchHeader = new IfNoneMatch();

        // @todo set some values, then test output
        $this->assertEmpty('If-None-Match: xxx', $ifNoneMatchHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        IfNoneMatch::fromString("If-None-Match: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new IfNoneMatch("xxx\r\n\r\nevilContent");
    }
}

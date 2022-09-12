<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\ContentEncoding;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class ContentEncodingTest extends TestCase
{
    public function testContentEncodingFromStringCreatesValidContentEncodingHeader()
    {
        $contentEncodingHeader = ContentEncoding::fromString('Content-Encoding: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $contentEncodingHeader);
        $this->assertInstanceOf(ContentEncoding::class, $contentEncodingHeader);
    }

    public function testContentEncodingGetFieldNameReturnsHeaderName()
    {
        $contentEncodingHeader = new ContentEncoding();
        $this->assertEquals('Content-Encoding', $contentEncodingHeader->getFieldName());
    }

    public function testContentEncodingGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('ContentEncoding needs to be completed');

        $contentEncodingHeader = new ContentEncoding();
        $this->assertEquals('xxx', $contentEncodingHeader->getFieldValue());
    }

    public function testContentEncodingToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('ContentEncoding needs to be completed');

        $contentEncodingHeader = new ContentEncoding();

        // @todo set some values, then test output
        $this->assertEmpty('Content-Encoding: xxx', $contentEncodingHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ContentEncoding::fromString("Content-Encoding: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new ContentEncoding("xxx\r\n\r\nevilContent");
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\ContentDisposition;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class ContentDispositionTest extends TestCase
{
    public function testContentDispositionFromStringCreatesValidContentDispositionHeader()
    {
        $contentDispositionHeader = ContentDisposition::fromString('Content-Disposition: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $contentDispositionHeader);
        $this->assertInstanceOf(ContentDisposition::class, $contentDispositionHeader);
    }

    public function testContentDispositionGetFieldNameReturnsHeaderName()
    {
        $contentDispositionHeader = new ContentDisposition();
        $this->assertEquals('Content-Disposition', $contentDispositionHeader->getFieldName());
    }

    public function testContentDispositionGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('ContentDisposition needs to be completed');

        $contentDispositionHeader = new ContentDisposition();
        $this->assertEquals('xxx', $contentDispositionHeader->getFieldValue());
    }

    public function testContentDispositionToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('ContentDisposition needs to be completed');

        $contentDispositionHeader = new ContentDisposition();

        // @todo set some values, then test output
        $this->assertEmpty('Content-Disposition: xxx', $contentDispositionHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ContentDisposition::fromString("Content-Disposition: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new ContentDisposition("xxx\r\n\r\nevilContent");
    }
}

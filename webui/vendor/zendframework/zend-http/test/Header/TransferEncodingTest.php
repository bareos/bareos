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
use Zend\Http\Header\TransferEncoding;

class TransferEncodingTest extends TestCase
{
    public function testTransferEncodingFromStringCreatesValidTransferEncodingHeader()
    {
        $transferEncodingHeader = TransferEncoding::fromString('Transfer-Encoding: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $transferEncodingHeader);
        $this->assertInstanceOf(TransferEncoding::class, $transferEncodingHeader);
    }

    public function testTransferEncodingGetFieldNameReturnsHeaderName()
    {
        $transferEncodingHeader = new TransferEncoding();
        $this->assertEquals('Transfer-Encoding', $transferEncodingHeader->getFieldName());
    }

    public function testTransferEncodingGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('TransferEncoding needs to be completed');

        $transferEncodingHeader = new TransferEncoding();
        $this->assertEquals('xxx', $transferEncodingHeader->getFieldValue());
    }

    public function testTransferEncodingToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('TransferEncoding needs to be completed');

        $transferEncodingHeader = new TransferEncoding();

        // @todo set some values, then test output
        $this->assertEmpty('Transfer-Encoding: xxx', $transferEncodingHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        TransferEncoding::fromString("Transfer-Encoding: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new TransferEncoding("xxx\r\n\r\nevilContent");
    }
}

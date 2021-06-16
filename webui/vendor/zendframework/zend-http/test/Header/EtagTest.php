<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Etag;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class EtagTest extends TestCase
{
    public function testEtagFromStringCreatesValidEtagHeader()
    {
        $etagHeader = Etag::fromString('Etag: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $etagHeader);
        $this->assertInstanceOf(Etag::class, $etagHeader);
    }

    public function testEtagGetFieldNameReturnsHeaderName()
    {
        $etagHeader = new Etag();
        $this->assertEquals('Etag', $etagHeader->getFieldName());
    }

    public function testEtagGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('Etag needs to be completed');

        $etagHeader = new Etag();
        $this->assertEquals('xxx', $etagHeader->getFieldValue());
    }

    public function testEtagToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Etag needs to be completed');

        $etagHeader = new Etag();

        // @todo set some values, then test output
        $this->assertEmpty('Etag: xxx', $etagHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Etag::fromString("Etag: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Etag("xxx\r\n\r\nevilContent");
    }
}

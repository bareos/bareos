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
use Zend\Http\Header\Vary;

class VaryTest extends TestCase
{
    public function testVaryFromStringCreatesValidVaryHeader()
    {
        $varyHeader = Vary::fromString('Vary: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $varyHeader);
        $this->assertInstanceOf(Vary::class, $varyHeader);
    }

    public function testVaryGetFieldNameReturnsHeaderName()
    {
        $varyHeader = new Vary();
        $this->assertEquals('Vary', $varyHeader->getFieldName());
    }

    public function testVaryGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('Vary needs to be completed');

        $varyHeader = new Vary();
        $this->assertEquals('xxx', $varyHeader->getFieldValue());
    }

    public function testVaryToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Vary needs to be completed');

        $varyHeader = new Vary();

        // @todo set some values, then test output
        $this->assertEmpty('Vary: xxx', $varyHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Vary::fromString("Vary: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Vary("xxx\r\n\r\nevilContent");
    }
}

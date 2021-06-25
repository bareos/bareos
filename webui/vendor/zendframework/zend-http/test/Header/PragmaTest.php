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
use Zend\Http\Header\Pragma;

class PragmaTest extends TestCase
{
    public function testPragmaFromStringCreatesValidPragmaHeader()
    {
        $pragmaHeader = Pragma::fromString('Pragma: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $pragmaHeader);
        $this->assertInstanceOf(Pragma::class, $pragmaHeader);
    }

    public function testPragmaGetFieldNameReturnsHeaderName()
    {
        $pragmaHeader = new Pragma();
        $this->assertEquals('Pragma', $pragmaHeader->getFieldName());
    }

    public function testPragmaGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('Pragma needs to be completed');

        $pragmaHeader = new Pragma();
        $this->assertEquals('xxx', $pragmaHeader->getFieldValue());
    }

    public function testPragmaToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Pragma needs to be completed');

        $pragmaHeader = new Pragma();

        // @todo set some values, then test output
        $this->assertEmpty('Pragma: xxx', $pragmaHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Pragma::fromString("Pragma: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Pragma("xxx\r\n\r\nevilContent");
    }
}

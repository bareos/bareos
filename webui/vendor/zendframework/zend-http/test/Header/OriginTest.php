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
use Zend\Http\Header\Origin;
use Zend\Uri\Exception\InvalidUriPartException;

class OriginTest extends TestCase
{
    /**
     * @group 6484
     */
    public function testOriginFieldValueIsAlwaysAString()
    {
        $origin = new Origin();

        $this->assertInternalType('string', $origin->getFieldValue());
    }

    public function testOriginFromStringCreatesValidOriginHeader()
    {
        $originHeader = Origin::fromString('Origin: http://zend.org');
        $this->assertInstanceOf(HeaderInterface::class, $originHeader);
        $this->assertInstanceOf(Origin::class, $originHeader);
    }

    public function testOriginGetFieldNameReturnsHeaderName()
    {
        $originHeader = new Origin();
        $this->assertEquals('Origin', $originHeader->getFieldName());
    }

    public function testOriginGetFieldValueReturnsProperValue()
    {
        $originHeader = Origin::fromString('Origin: http://zend.org');
        $this->assertEquals('http://zend.org', $originHeader->getFieldValue());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidUriPartException::class);
        Origin::fromString("Origin: http://zend.org\r\n\r\nevilContent");
    }

    /**
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Origin("http://zend.org\r\n\r\nevilContent");
    }
}

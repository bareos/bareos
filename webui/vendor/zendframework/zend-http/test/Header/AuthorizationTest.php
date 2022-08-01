<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Authorization;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class AuthorizationTest extends TestCase
{
    public function testAuthorizationFromStringCreatesValidAuthorizationHeader()
    {
        $authorizationHeader = Authorization::fromString('Authorization: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $authorizationHeader);
        $this->assertInstanceOf(Authorization::class, $authorizationHeader);
    }

    public function testAuthorizationGetFieldNameReturnsHeaderName()
    {
        $authorizationHeader = new Authorization();
        $this->assertEquals('Authorization', $authorizationHeader->getFieldName());
    }

    public function testAuthorizationGetFieldValueReturnsProperValue()
    {
        $authorizationHeader = new Authorization('xxx');
        $this->assertEquals('xxx', $authorizationHeader->getFieldValue());
    }

    public function testAuthorizationToStringReturnsHeaderFormattedString()
    {
        $authorizationHeader = new Authorization('xxx');
        $this->assertEquals('Authorization: xxx', $authorizationHeader->toString());

        $authorizationHeader = Authorization::fromString('Authorization: xxx2');
        $this->assertEquals('Authorization: xxx2', $authorizationHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        $header = Authorization::fromString("Authorization: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Authorization("xxx\r\n\r\nevilContent");
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\AuthenticationInfo;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class AuthenticationInfoTest extends TestCase
{
    public function testAuthenticationInfoFromStringCreatesValidAuthenticationInfoHeader()
    {
        $authenticationInfoHeader = AuthenticationInfo::fromString('Authentication-Info: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $authenticationInfoHeader);
        $this->assertInstanceOf(AuthenticationInfo::class, $authenticationInfoHeader);
    }

    public function testAuthenticationInfoGetFieldNameReturnsHeaderName()
    {
        $authenticationInfoHeader = new AuthenticationInfo();
        $this->assertEquals('Authentication-Info', $authenticationInfoHeader->getFieldName());
    }

    public function testAuthenticationInfoGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('AuthenticationInfo needs to be completed');

        $authenticationInfoHeader = new AuthenticationInfo();
        $this->assertEquals('xxx', $authenticationInfoHeader->getFieldValue());
    }

    public function testAuthenticationInfoToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('AuthenticationInfo needs to be completed');

        $authenticationInfoHeader = new AuthenticationInfo();

        // @todo set some values, then test output
        $this->assertEmpty('Authentication-Info: xxx', $authenticationInfoHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        $header = AuthenticationInfo::fromString("Authentication-Info: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new AuthenticationInfo("xxx\r\n\r\nevilContent");
    }
}

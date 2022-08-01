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
use Zend\Http\Header\ProxyAuthorization;

class ProxyAuthorizationTest extends TestCase
{
    public function testProxyAuthorizationFromStringCreatesValidProxyAuthorizationHeader()
    {
        $proxyAuthorizationHeader = ProxyAuthorization::fromString('Proxy-Authorization: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $proxyAuthorizationHeader);
        $this->assertInstanceOf(ProxyAuthorization::class, $proxyAuthorizationHeader);
    }

    public function testProxyAuthorizationGetFieldNameReturnsHeaderName()
    {
        $proxyAuthorizationHeader = new ProxyAuthorization();
        $this->assertEquals('Proxy-Authorization', $proxyAuthorizationHeader->getFieldName());
    }

    public function testProxyAuthorizationGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('ProxyAuthorization needs to be completed');

        $proxyAuthorizationHeader = new ProxyAuthorization();
        $this->assertEquals('xxx', $proxyAuthorizationHeader->getFieldValue());
    }

    public function testProxyAuthorizationToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('ProxyAuthorization needs to be completed');

        $proxyAuthorizationHeader = new ProxyAuthorization();

        // @todo set some values, then test output
        $this->assertEmpty('Proxy-Authorization: xxx', $proxyAuthorizationHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ProxyAuthorization::fromString("Proxy-Authorization: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new ProxyAuthorization("xxx\r\n\r\nevilContent");
    }
}

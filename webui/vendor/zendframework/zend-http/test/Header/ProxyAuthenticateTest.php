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
use Zend\Http\Header\ProxyAuthenticate;

class ProxyAuthenticateTest extends TestCase
{
    public function testProxyAuthenticateFromStringCreatesValidProxyAuthenticateHeader()
    {
        $proxyAuthenticateHeader = ProxyAuthenticate::fromString('Proxy-Authenticate: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $proxyAuthenticateHeader);
        $this->assertInstanceOf(ProxyAuthenticate::class, $proxyAuthenticateHeader);
    }

    public function testProxyAuthenticateGetFieldNameReturnsHeaderName()
    {
        $proxyAuthenticateHeader = new ProxyAuthenticate();
        $this->assertEquals('Proxy-Authenticate', $proxyAuthenticateHeader->getFieldName());
    }

    public function testProxyAuthenticateGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('ProxyAuthenticate needs to be completed');

        $proxyAuthenticateHeader = new ProxyAuthenticate();
        $this->assertEquals('xxx', $proxyAuthenticateHeader->getFieldValue());
    }

    public function testProxyAuthenticateToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('ProxyAuthenticate needs to be completed');

        $proxyAuthenticateHeader = new ProxyAuthenticate();

        // @todo set some values, then test output
        $this->assertEmpty('Proxy-Authenticate: xxx', $proxyAuthenticateHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ProxyAuthenticate::fromString("Proxy-Authenticate: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new ProxyAuthenticate("xxx\r\n\r\nevilContent");
    }
}

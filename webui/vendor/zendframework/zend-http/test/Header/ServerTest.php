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
use Zend\Http\Header\Server;

class ServerTest extends TestCase
{
    public function testServerFromStringCreatesValidServerHeader()
    {
        $serverHeader = Server::fromString('Server: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $serverHeader);
        $this->assertInstanceOf(Server::class, $serverHeader);
    }

    public function testServerGetFieldNameReturnsHeaderName()
    {
        $serverHeader = new Server();
        $this->assertEquals('Server', $serverHeader->getFieldName());
    }

    public function testServerGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('Server needs to be completed');

        $serverHeader = new Server();
        $this->assertEquals('xxx', $serverHeader->getFieldValue());
    }

    public function testServerToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Server needs to be completed');

        $serverHeader = new Server();

        // @todo set some values, then test output
        $this->assertEmpty('Server: xxx', $serverHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Server::fromString("Server: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new Server("xxx\r\n\r\nevilContent");
    }
}

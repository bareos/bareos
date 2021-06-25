<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Connection;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class ConnectionTest extends TestCase
{
    public function testConnectionFromStringCreatesValidConnectionHeader()
    {
        $connectionHeader = Connection::fromString('Connection: close');
        $this->assertInstanceOf(HeaderInterface::class, $connectionHeader);
        $this->assertInstanceOf(Connection::class, $connectionHeader);
        $this->assertEquals('close', $connectionHeader->getFieldValue());
        $this->assertFalse($connectionHeader->isPersistent());
    }

    public function testConnectionGetFieldNameReturnsHeaderName()
    {
        $connectionHeader = new Connection();
        $this->assertEquals('Connection', $connectionHeader->getFieldName());
    }

    public function testConnectionGetFieldValueReturnsProperValue()
    {
        $connectionHeader = new Connection();
        $connectionHeader->setValue('Keep-Alive');
        $this->assertEquals('keep-alive', $connectionHeader->getFieldValue());
    }

    public function testConnectionToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('Connection needs to be completed');

        $connectionHeader = new Connection();
        $connectionHeader->setValue('close');
        $this->assertEmpty('Connection: close', $connectionHeader->toString());
    }

    public function testConnectionSetPersistentReturnsProperValue()
    {
        $connectionHeader = new Connection();
        $connectionHeader->setPersistent(true);
        $this->assertEquals('keep-alive', $connectionHeader->getFieldValue());
        $connectionHeader->setPersistent(false);
        $this->assertEquals('close', $connectionHeader->getFieldValue());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Connection::fromString("Connection: close\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaSetters()
    {
        $header = new Connection();
        $this->expectException(InvalidArgumentException::class);
        $header->setValue("close\r\n\r\nevilContent");
    }
}

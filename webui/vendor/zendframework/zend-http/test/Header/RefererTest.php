<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\GenericHeader;
use Zend\Http\Header\HeaderInterface;
use Zend\Http\Header\Referer;
use Zend\Http\Headers;
use Zend\Uri\Http;
use Zend\Uri\Uri;

class RefererTest extends TestCase
{
    public function testRefererFromStringCreatesValidLocationHeader()
    {
        $refererHeader = Referer::fromString('Referer: http://www.example.com/');
        $this->assertInstanceOf(HeaderInterface::class, $refererHeader);
        $this->assertInstanceOf(Referer::class, $refererHeader);
    }

    public function testRefererGetFieldValueReturnsProperValue()
    {
        $refererHeader = new Referer();
        $refererHeader->setUri('http://www.example.com/');
        $this->assertEquals('http://www.example.com/', $refererHeader->getFieldValue());

        $refererHeader->setUri('/path');
        $this->assertEquals('/path', $refererHeader->getFieldValue());
    }

    public function testRefererToStringReturnsHeaderFormattedString()
    {
        $refererHeader = new Referer();
        $refererHeader->setUri('http://www.example.com/path?query');

        $this->assertEquals('Referer: http://www.example.com/path?query', $refererHeader->toString());
    }

    /** Implementation specific tests here */

    public function testRefererCanSetAndAccessAbsoluteUri()
    {
        $refererHeader = Referer::fromString('Referer: http://www.example.com/path');
        $uri = $refererHeader->uri();
        $this->assertInstanceOf(Http::class, $uri);
        $this->assertTrue($uri->isAbsolute());
        $this->assertEquals('http://www.example.com/path', $refererHeader->getUri());
    }

    public function testRefererCanSetAndAccessRelativeUri()
    {
        $refererHeader = Referer::fromString('Referer: /path/to');
        $uri = $refererHeader->uri();
        $this->assertInstanceOf(Uri::class, $uri);
        $this->assertFalse($uri->isAbsolute());
        $this->assertEquals('/path/to', $refererHeader->getUri());
    }

    public function testRefererDoesNotHaveUriFragment()
    {
        $refererHeader = new Referer();
        $refererHeader->setUri('http://www.example.com/path?query#fragment');
        $this->assertEquals('Referer: http://www.example.com/path?query', $refererHeader->toString());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testCRLFAttack()
    {
        $this->expectException(InvalidArgumentException::class);
        Referer::fromString("Referer: http://www.example.com/\r\n\r\nevilContent");
    }

    public function testInvalidUriShouldWrapException()
    {
        $headerString = "Referer: unknown-scheme://test";

        $headers = Headers::fromString($headerString);

        $result = $headers->get('Referer');

        $this->assertInstanceOf(GenericHeader::class, $result);
        $this->assertNotInstanceOf(Referer::class, $result);
        $this->assertEquals('unknown-scheme://test', $result->getFieldValue());
    }
}

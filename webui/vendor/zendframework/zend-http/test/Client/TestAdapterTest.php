<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Client;

use PHPUnit\Framework\TestCase;
use Zend\Http\Client\Adapter\Exception\InvalidArgumentException;
use Zend\Http\Client\Adapter\Exception\OutOfRangeException;
use Zend\Http\Client\Adapter\Exception\RuntimeException;
use Zend\Http\Client\Adapter\Test;
use Zend\Http\Response;

/**
 * Exercises Zend_Http_Client_Adapter_Test
 *
 * @group      Zend_Http
 * @group      Zend_Http_Client
 */
class TestAdapterTest extends TestCase
{
    /**
     * Test adapter
     *
     * @var Test
     */
    protected $adapter;

    /**
     * Set up the test adapter before running the test
     */
    public function setUp()
    {
        $this->adapter = new Test();
    }

    /**
     * Tear down the test adapter after running the test
     */
    public function tearDown()
    {
        $this->adapter = null;
    }

    /**
     * Make sure an exception is thrown on invalid configuration
     */
    public function testSetConfigThrowsOnInvalidConfig()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Array or Traversable object expected');

        $this->adapter->setOptions('foo');
    }

    public function testSetConfigReturnsQuietly()
    {
        $this->adapter->setOptions(['foo' => 'bar']);
    }

    public function testConnectReturnsQuietly()
    {
        $this->adapter->connect('http://foo');
    }

    public function testCloseReturnsQuietly()
    {
        $this->adapter->close();
    }

    public function testFailRequestOnDemand()
    {
        $this->adapter->setNextRequestWillFail(true);

        try {
            // Make a connection that will fail
            $this->adapter->connect('http://foo');
            $this->fail();
        } catch (RuntimeException $e) {
            // Connect again to see that the next request does not fail
            $this->adapter->connect('http://foo');
        }
    }

    public function testReadDefaultResponse()
    {
        $expected = "HTTP/1.1 400 Bad Request\r\n\r\n";
        $this->assertEquals($expected, $this->adapter->read());
    }

    public function testReadingSingleResponse()
    {
        $expected = "HTTP/1.1 200 OK\r\n\r\n";
        $this->adapter->setResponse($expected);
        $this->assertEquals($expected, $this->adapter->read());
        $this->assertEquals($expected, $this->adapter->read());
    }

    public function testReadingResponseCycles()
    {
        $expected = [
            "HTTP/1.1 200 OK\r\n\r\n",
            "HTTP/1.1 302 Moved Temporarily\r\n\r\n",
        ];

        $this->adapter->setResponse($expected[0]);
        $this->adapter->addResponse($expected[1]);

        $this->assertEquals($expected[0], $this->adapter->read());
        $this->assertEquals($expected[1], $this->adapter->read());
        $this->assertEquals($expected[0], $this->adapter->read());
    }

    /**
     * Test that responses could be added as strings
     *
     * @dataProvider validHttpResponseProvider
     *
     * @param string $testResponse
     */
    public function testAddResponseAsString($testResponse)
    {
        $this->adapter->read(); // pop out first response

        $this->adapter->addResponse($testResponse);
        $this->assertEquals($testResponse, $this->adapter->read());
    }

    /**
     * Test that responses could be added as objects (ZF-7009)
     *
     * @link http://framework.zend.com/issues/browse/ZF-7009
     *
     * @dataProvider validHttpResponseProvider
     *
     * @param string $testResponse
     */
    public function testAddResponseAsObject($testResponse)
    {
        $this->adapter->read(); // pop out first response

        $respObj = Response::fromString($testResponse);

        $this->adapter->addResponse($respObj);
        $this->assertEquals($testResponse, $this->adapter->read());
    }

    public function testReadingResponseCyclesWhenSetByArray()
    {
        $expected = [
            "HTTP/1.1 200 OK\r\n\r\n",
            "HTTP/1.1 302 Moved Temporarily\r\n\r\n",
        ];

        $this->adapter->setResponse($expected);

        $this->assertEquals($expected[0], $this->adapter->read());
        $this->assertEquals($expected[1], $this->adapter->read());
        $this->assertEquals($expected[0], $this->adapter->read());
    }

    public function testSettingNextResponseByIndex()
    {
        $expected = [
            "HTTP/1.1 200 OK\r\n\r\n",
            "HTTP/1.1 302 Moved Temporarily\r\n\r\n",
            "HTTP/1.1 404 Not Found\r\n\r\n",
        ];

        $this->adapter->setResponse($expected);
        $this->assertEquals($expected[0], $this->adapter->read());

        foreach ($expected as $i => $expected) {
            $this->adapter->setResponseIndex($i);
            $this->assertEquals($expected, $this->adapter->read());
        }
    }

    public function testSettingNextResponseToAnInvalidIndex()
    {
        $indexes = [-1, 1];
        foreach ($indexes as $i) {
            try {
                $this->adapter->setResponseIndex($i);
                $this->fail();
            } catch (\Exception $e) {
                $this->assertInstanceOf(OutOfRangeException::class, $e);
                $this->assertRegexp('/out of range/i', $e->getMessage());
            }
        }
    }

    /**
     * Data Providers
     */

    /**
     * Provide valid HTTP responses as string
     *
     * @return array
     */
    public static function validHttpResponseProvider()
    {
        return [
            ['HTTP/1.1 200 OK' . "\r\n\r\n"],
            [
                'HTTP/1.1 302 Moved Temporarily' . "\r\n"
                . 'Location: http://example.com/baz' . "\r\n\r\n",
            ],
            [
                'HTTP/1.1 404 Not Found' . "\r\n"
                . 'Date: Sun, 14 Jun 2009 10:40:06 GMT' . "\r\n"
                . 'Server: Apache/2.2.3 (CentOS)' . "\r\n"
                . 'Content-length: 281' . "\r\n"
                . 'Connection: close' . "\r\n"
                . 'Content-type: text/html; charset=iso-8859-1' . "\r\n\r\n"
                . '<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML 2.0//EN">' . "\n"
                . '<html><head>' . "\n"
                . '<title>404 Not Found</title>' . "\n"
                . '</head><body>' . "\n"
                . '<h1>Not Found</h1>' . "\n"
                . '<p>The requested URL /foo/bar was not found on this server.</p>' . "\n"
                . '<hr>' . "\n"
                . '<address>Apache/2.2.3 (CentOS) Server at example.com Port 80</address>' . "\n"
                . '</body></html>',
            ],
        ];
    }
}

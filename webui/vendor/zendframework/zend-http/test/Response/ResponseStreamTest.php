<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Response;

use PHPUnit\Framework\TestCase;
use Zend\Http\Response\Stream;

class ResponseStreamTest extends TestCase
{
    public function testResponseFactoryFromStringCreatesValidResponse()
    {
        $string = 'HTTP/1.0 200 OK' . "\r\n\r\n" . 'Foo Bar' . "\r\n";
        $stream = fopen('php://temp', 'rb+');
        fwrite($stream, 'Bar Foo');
        rewind($stream);

        $response = Stream::fromStream($string, $stream);
        $this->assertEquals(200, $response->getStatusCode());
        $this->assertEquals("Foo Bar\r\nBar Foo", $response->getBody());
    }

    /**
     * @group 6027
     *
     * @covers \Zend\Http\Response\Stream::fromStream
     */
    public function testResponseFactoryFromEmptyStringCreatesValidResponse()
    {
        $stream = fopen('php://temp', 'rb+');
        fwrite($stream, 'HTTP/1.0 200 OK' . "\r\n\r\n" . 'Foo Bar' . "\r\n" . 'Bar Foo');
        rewind($stream);

        $response = Stream::fromStream('', $stream);
        $this->assertEquals(200, $response->getStatusCode());
        $this->assertEquals("Foo Bar\r\nBar Foo", $response->getBody());
    }

    public function testGzipResponse()
    {
        $stream = fopen(__DIR__ . '/../_files/response_gzip', 'rb');

        $headers = '';
        while (false !== ($newLine = fgets($stream))) {
            $headers .= $newLine;
            if ($headers == "\n" || $headers == "\r\n") {
                break;
            }
        }

        $headers .= fread($stream, 100); // Should accept also part of body as text

        $res = Stream::fromStream($headers, $stream);

        $this->assertEquals('gzip', $res->getHeaders()->get('Content-encoding')->getFieldValue());
        $this->assertEquals('0b13cb193de9450aa70a6403e2c9902f', md5($res->getBody()));
        $this->assertEquals('f24dd075ba2ebfb3bf21270e3fdc5303', md5($res->getContent()));
    }

    public function test300isRedirect()
    {
        $values   = $this->readResponse('response_302');
        $response = Stream::fromStream($values['data'], $values['stream']);

        $this->assertEquals(302, $response->getStatusCode(), 'Response code is expected to be 302, but it\'s not.');
        $this->assertFalse($response->isClientError(), 'Response is an error, but isClientError() returned true');
        $this->assertFalse($response->isForbidden(), 'Response is an error, but isForbidden() returned true');
        $this->assertFalse($response->isInformational(), 'Response is an error, but isInformational() returned true');
        $this->assertFalse($response->isNotFound(), 'Response is an error, but isNotFound() returned true');
        $this->assertFalse($response->isOk(), 'Response is an error, but isOk() returned true');
        $this->assertFalse($response->isServerError(), 'Response is an error, but isServerError() returned true');
        $this->assertTrue($response->isRedirect(), 'Response is an error, but isRedirect() returned false');
        $this->assertFalse($response->isSuccess(), 'Response is an error, but isSuccess() returned true');
    }

    /**
     * Helper function: read test response from file
     *
     * @param string $response
     * @return string
     */
    protected function readResponse($response)
    {
        $stream = fopen(__DIR__ . DIRECTORY_SEPARATOR . '..' . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR
            . $response, 'rb');

        $data = '';
        while (false !== ($newLine = fgets($stream))) {
            $data .= $newLine;
            if ($newLine == "\n" || $newLine == "\r\n") {
                break;
            }
        }

        $data .= fread($stream, 100); // Should accept also part of body as text

        $return = [];
        $return['stream'] = $stream;
        $return['data']   = $data;

        return $return;
    }
}

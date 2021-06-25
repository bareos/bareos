<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http;

use PHPUnit\Framework\TestCase;
use ReflectionClass;
use stdClass;
use Zend\Http\Exception\InvalidArgumentException;
use Zend\Http\Exception\RuntimeException;
use Zend\Http\Header\GenericHeader;
use Zend\Http\Headers;
use Zend\Http\Request;
use Zend\Stdlib\Parameters;
use Zend\Uri\Uri;

class RequestTest extends TestCase
{
    public function testRequestFromStringFactoryCreatesValidRequest()
    {
        $string = "GET /foo?myparam=myvalue HTTP/1.1\r\n\r\nSome Content";
        $request = Request::fromString($string);

        $this->assertEquals(Request::METHOD_GET, $request->getMethod());
        $this->assertEquals('/foo?myparam=myvalue', $request->getUri());
        $this->assertEquals('myvalue', $request->getQuery()->get('myparam'));
        $this->assertEquals(Request::VERSION_11, $request->getVersion());
        $this->assertEquals('Some Content', $request->getContent());
    }

    public function testRequestUsesParametersContainerByDefault()
    {
        $request = new Request();
        $this->assertInstanceOf(Parameters::class, $request->getQuery());
        $this->assertInstanceOf(Parameters::class, $request->getPost());
        $this->assertInstanceOf(Parameters::class, $request->getFiles());
    }

    public function testRequestAllowsSettingOfParameterContainer()
    {
        $request = new Request();
        $p = new Parameters();
        $request->setQuery($p);
        $request->setPost($p);
        $request->setFiles($p);

        $this->assertSame($p, $request->getQuery());
        $this->assertSame($p, $request->getPost());
        $this->assertSame($p, $request->getFiles());

        $headers = new Headers();
        $request->setHeaders($headers);
        $this->assertSame($headers, $request->getHeaders());
    }

    public function testRetrievingASingleValueForParameters()
    {
        $request = new Request();
        $p = new Parameters([
            'foo' => 'bar',
        ]);
        $request->setQuery($p);
        $request->setPost($p);
        $request->setFiles($p);

        $this->assertSame('bar', $request->getQuery('foo'));
        $this->assertSame('bar', $request->getPost('foo'));
        $this->assertSame('bar', $request->getFiles('foo'));

        $headers = new Headers();
        $h = new GenericHeader('foo', 'bar');
        $headers->addHeader($h);

        $request->setHeaders($headers);
        $this->assertSame($headers, $request->getHeaders());
        $this->assertSame($h, $request->getHeaders()->get('foo'));
        $this->assertSame($h, $request->getHeader('foo'));
    }

    public function testParameterRetrievalDefaultValue()
    {
        $request = new Request();
        $p = new Parameters([
            'foo' => 'bar',
        ]);
        $request->setQuery($p);
        $request->setPost($p);
        $request->setFiles($p);

        $default = 15;
        $this->assertSame($default, $request->getQuery('baz', $default));
        $this->assertSame($default, $request->getPost('baz', $default));
        $this->assertSame($default, $request->getFiles('baz', $default));
        $this->assertSame($default, $request->getHeaders('baz', $default));
        $this->assertSame($default, $request->getHeader('baz', $default));
    }

    public function testRequestPersistsRawBody()
    {
        $request = new Request();
        $request->setContent('foo');
        $this->assertEquals('foo', $request->getContent());
    }

    public function testRequestUsesHeadersContainerByDefault()
    {
        $request = new Request();
        $this->assertInstanceOf(Headers::class, $request->getHeaders());
    }

    public function testRequestCanSetHeaders()
    {
        $request = new Request();
        $headers = new Headers();

        $ret = $request->setHeaders($headers);
        $this->assertInstanceOf(Request::class, $ret);
        $this->assertSame($headers, $request->getHeaders());
    }

    public function testRequestCanSetAndRetrieveValidMethod()
    {
        $request = new Request();
        $request->setMethod('POST');
        $this->assertEquals('POST', $request->getMethod());
    }

    public function testRequestCanAlwaysForcesUppecaseMethodName()
    {
        $request = new Request();
        $request->setMethod('get');
        $this->assertEquals('GET', $request->getMethod());
    }

    /**
     * @dataProvider uriDataProvider
     *
     * @param string $uri
     */
    public function testRequestCanSetAndRetrieveUri($uri)
    {
        $request = new Request();
        $request->setUri($uri);
        $this->assertEquals($uri, $request->getUri());
        $this->assertInstanceOf(Uri::class, $request->getUri());
        $this->assertEquals($uri, $request->getUri()->toString());
        $this->assertEquals($uri, $request->getUriString());
    }

    public function uriDataProvider()
    {
        return [
            ['/foo'],
            ['/foo#test'],
            ['/hello?what=true#noway'],
        ];
    }

    public function testRequestSetUriWillThrowExceptionOnInvalidArgument()
    {
        $request = new Request();

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('must be an instance of');
        $request->setUri(new stdClass());
    }

    public function testRequestCanSetAndRetrieveVersion()
    {
        $request = new Request();
        $this->assertEquals('1.1', $request->getVersion());
        $request->setVersion(Request::VERSION_10);
        $this->assertEquals('1.0', $request->getVersion());
    }

    public function testRequestSetVersionWillThrowExceptionOnInvalidArgument()
    {
        $request = new Request();

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Not valid or not supported HTTP version');
        $request->setVersion('1.2');
    }

    /**
     * @dataProvider getMethods
     *
     * @param string $methodName
     */
    public function testRequestMethodCheckWorksForAllMethods($methodName)
    {
        $request = new Request();
        $request->setMethod($methodName);

        foreach ($this->getMethods(false, $methodName) as $testMethodName => $testMethodValue) {
            $this->assertEquals($testMethodValue, $request->{'is' . $testMethodName}());
        }
    }

    public function testRequestCanBeCastToAString()
    {
        $request = new Request();
        $request->setMethod(Request::METHOD_GET);
        $request->setUri('/');
        $request->setContent('foo=bar&bar=baz');
        $this->assertEquals("GET / HTTP/1.1\r\n\r\nfoo=bar&bar=baz", $request->toString());
    }

    public function testRequestIsXmlHttpRequest()
    {
        $request = new Request();
        $this->assertFalse($request->isXmlHttpRequest());

        $request = new Request();
        $request->getHeaders()->addHeaderLine('X_REQUESTED_WITH', 'FooBazBar');
        $this->assertFalse($request->isXmlHttpRequest());

        $request = new Request();
        $request->getHeaders()->addHeaderLine('X_REQUESTED_WITH', 'XMLHttpRequest');
        $this->assertTrue($request->isXmlHttpRequest());
    }

    public function testRequestIsFlashRequest()
    {
        $request = new Request();
        $this->assertFalse($request->isFlashRequest());

        $request = new Request();
        $request->getHeaders()->addHeaderLine('USER_AGENT', 'FooBazBar');
        $this->assertFalse($request->isFlashRequest());

        $request = new Request();
        $request->getHeaders()->addHeaderLine('USER_AGENT', 'Shockwave Flash');
        $this->assertTrue($request->isFlashRequest());
    }

    /**
     * @group 4893
     */
    public function testRequestsWithoutHttpVersionAreOK()
    {
        $requestString = 'GET http://www.domain.com/index.php';
        $request = Request::fromString($requestString);
        $this->assertEquals($request::METHOD_GET, $request->getMethod());
    }

    /**
     * @param bool $providerContext
     * @param null|string $trueMethod
     * @return array
     */
    public function getMethods($providerContext, $trueMethod = null)
    {
        $refClass = new ReflectionClass(Request::class);
        $return = [];
        foreach ($refClass->getConstants() as $cName => $cValue) {
            if (substr($cName, 0, 6) == 'METHOD') {
                if ($providerContext) {
                    $return[] = [$cValue];
                } else {
                    $return[strtolower($cValue)] = $trueMethod == $cValue;
                }
            }
        }
        return $return;
    }

    public function testCustomMethods()
    {
        $request = new Request();
        $this->assertTrue($request->getAllowCustomMethods());
        $request->setMethod('xcustom');

        $this->assertEquals('XCUSTOM', $request->getMethod());
    }

    public function testDisallowCustomMethods()
    {
        $request = new Request();
        $request->setAllowCustomMethods(false);

        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid HTTP method passed');

        $request->setMethod('xcustom');
    }

    public function testCustomMethodsFromString()
    {
        $request = Request::fromString('X-CUS_TOM someurl');
        $this->assertTrue($request->getAllowCustomMethods());

        $this->assertEquals('X-CUS_TOM', $request->getMethod());
    }

    public function testDisallowCustomMethodsFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('A valid request line was not found in the provided string');

        Request::fromString('X-CUS_TOM someurl', false);
    }

    public function testAllowCustomMethodsFlagIsSetByFromString()
    {
        $request = Request::fromString('GET someurl', false);
        $this->assertFalse($request->getAllowCustomMethods());
    }

    public function testFromStringFactoryCreatesSingleObjectWithHeaderFolding()
    {
        $request = Request::fromString("GET /foo HTTP/1.1\r\nFake: foo\r\n -bar");
        $headers = $request->getHeaders();
        $this->assertEquals(1, $headers->count());

        $header = $headers->get('fake');
        $this->assertInstanceOf(GenericHeader::class, $header);
        $this->assertEquals('Fake', $header->getFieldName());
        $this->assertEquals('foo-bar', $header->getFieldValue());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testCRLFAttack()
    {
        $this->expectException(RuntimeException::class);
        Request::fromString(
            "GET /foo HTTP/1.1\r\nHost: example.com\r\nX-Foo: This\ris\r\n\r\nCRLF\nInjection"
        );
    }

    public function testGetHeadersDoesNotRaiseExceptionForInvalidHeaderLines()
    {
        $request = Request::fromString("GET /foo HTTP/1.1\r\nHost: example.com\r\nUseragent: h4ckerbot");

        $headers = $request->getHeaders();
        $this->assertFalse($headers->has('User-Agent'));
        $this->assertFalse($headers->get('User-Agent'));
        $this->assertSame('bar-baz', $request->getHeader('User-Agent', 'bar-baz'));

        $this->assertTrue($headers->has('useragent'));
        $this->assertInstanceOf(GenericHeader::class, $headers->get('useragent'));
        $this->assertSame('h4ckerbot', $headers->get('useragent')->getFieldValue());
        $this->assertSame('h4ckerbot', $request->getHeader('useragent')->getFieldValue());
    }
}

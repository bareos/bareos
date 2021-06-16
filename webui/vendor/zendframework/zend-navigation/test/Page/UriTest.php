<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Navigation\Page;

use PHPUnit\Framework\TestCase;
use Zend\Navigation\Page;
use Zend\Navigation;
use Zend\Http\Request;

/**
 * Tests the class Zend_Navigation_Page_Uri
 *
 * @group      Zend_Navigation
 */
class UriTest extends TestCase
{
    public function testUriOptionAsString()
    {
        $page = new Page\Uri([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $this->assertEquals('#', $page->getUri());
    }

    public function testUriOptionAsNull()
    {
        $page = new Page\Uri([
            'label' => 'foo',
            'uri' => null
        ]);

        $this->assertNull($page->getUri(), 'getUri() should return null');
    }

    public function testUriOptionAsInteger()
    {
        $this->expectException(
            Navigation\Exception\InvalidArgumentException::class
        );

        new Page\Uri(['uri' => 1337]);
    }

    public function testUriOptionAsObject()
    {
        $this->expectException(
            Navigation\Exception\InvalidArgumentException::class
        );

        $uri = new \stdClass();
        $uri->foo = 'bar';

        new Page\Uri(['uri' => $uri]);
    }

    public function testSetAndGetUri()
    {
        $page = new Page\Uri([
            'label' => 'foo',
            'uri' => '#'
        ]);

        $page->setUri('http://www.example.com/')->setUri('about:blank');

        $this->assertEquals('about:blank', $page->getUri());
    }

    public function testGetHref()
    {
        $uri = 'spotify:album:4YzcWwBUSzibRsqD9Sgu4A';

        $page = new Page\Uri();
        $page->setUri($uri);

        $this->assertEquals($uri, $page->getHref());
    }

    public function testIsActiveReturnsTrueWhenHasMatchingRequestUri()
    {
        $page = new Page\Uri([
            'label' => 'foo',
            'uri' => '/bar'
        ]);

        $request = new Request();
        $request->setUri('/bar');
        $request->setMethod('GET');

        $page->setRequest($request);

        $this->assertInstanceOf('Zend\Http\Request', $page->getRequest());

        $this->assertTrue($page->isActive());
    }

    public function testIsActiveReturnsFalseOnNonMatchingRequestUri()
    {
        $page = new Page\Uri([
            'label' => 'foo',
            'uri' => '/bar'
        ]);

        $request = new Request();
        $request->setUri('/baz');
        $request->setMethod('GET');

        $page->setRequest($request);

        $this->assertFalse($page->isActive());
    }

    /**
     * @group ZF-8922
     */
    public function testGetHrefWithFragmentIdentifier()
    {
        $uri = 'http://www.example.com/foo.html';

        $page = new Page\Uri();
        $page->setUri($uri);
        $page->setFragment('bar');

        $this->assertEquals($uri . '#bar', $page->getHref());

        $page->setUri('#');

        $this->assertEquals('#bar', $page->getHref());
    }
}

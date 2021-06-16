<?php
/**
 * @see       https://github.com/zendframework/zend-uri for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-uri/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Uri;

use Zend\Uri\Exception\InvalidUriPartException;
use Zend\Uri\Mailto as MailtoUri;
use PHPUnit\Framework\TestCase;

/**
 * @group      Zend_Uri
 * @group      Zend_Uri_Http
 * @group      Zend_Http
 */
class MailtoTest extends TestCase
{
    /**
     * Data Providers
     */

    /**
     * Valid schemes
     *
     * @return array
     */
    public function validSchemeProvider()
    {
        return [
            ['mailto'],
            ['MAILTO'],
            ['Mailto'],
        ];
    }

    /**
     * Invalid schemes
     *
     * @return array
     */
    public function invalidSchemeProvider()
    {
        return [
            ['file'],
            ['http'],
            ['g'],
            ['mailto:']
        ];
    }

    public function invalidUris()
    {
        return [
            ['mailto:/foo@example.com'],
            ['mailto://foo@example.com'],
            ['mailto:foo@example.com/bar/baz'],
            ['mailto:foo:bar@example.com/bar/baz'],
            ['mailto:foo:bar'],
        ];
    }

    /**
     * Tests
     */

    /**
     * Test that specific schemes are valid for this class
     *
     * @param string $scheme
     * @dataProvider validSchemeProvider
     */
    public function testValidScheme($scheme)
    {
        $uri = new MailtoUri;
        $uri->setScheme($scheme);
        $this->assertEquals($scheme, $uri->getScheme());
    }

    /**
     * Test that specific schemes are invalid for this class
     *
     * @param string $scheme
     * @dataProvider invalidSchemeProvider
     */
    public function testInvalidScheme($scheme)
    {
        $uri = new MailtoUri;
        $this->expectException(InvalidUriPartException::class);
        $uri->setScheme($scheme);
    }

    /**
     * Test that validateScheme returns false for schemes not valid for use
     * with the Mailto class
     *
     * @param string $scheme
     * @dataProvider invalidSchemeProvider
     */
    public function testValidateSchemeInvalid($scheme)
    {
        $this->assertFalse(MailtoUri::validateScheme($scheme));
    }

    public function testCapturesQueryString()
    {
        $uri = new MailtoUri('mailto:foo@example.com?Subject=Testing%20Subjects');
        $this->assertEquals('Subject=Testing%20Subjects', $uri->getQuery());
        $this->assertEquals(['Subject' => 'Testing Subjects'], $uri->getQueryAsArray());
    }

    public function testUserInfoIsNull()
    {
        $uri = new MailtoUri('mailto:foo@example.com?Subject=Testing%20Subjects');
        $this->assertNull($uri->getUserInfo());
    }

    public function testHostIsNull()
    {
        $uri = new MailtoUri('mailto:foo@example.com?Subject=Testing%20Subjects');
        $this->assertNull($uri->getHost());
    }

    public function testPortIsNull()
    {
        $uri = new MailtoUri('mailto:foo@example.com?Subject=Testing%20Subjects');
        $this->assertNull($uri->getPort());
    }

    public function testPathEquatesToEmail()
    {
        $uri = new MailtoUri('mailto:foo@example.com?Subject=Testing%20Subjects');
        $this->assertEquals('foo@example.com', $uri->getPath());
        $this->assertEquals('foo@example.com', $uri->getEmail());
        $this->assertEquals($uri->getEmail(), $uri->getPath());
    }

    /**
     * @dataProvider invalidUris
     */
    public function testInvalidMailtoUris($uri)
    {
        $uri = new MailtoUri($uri);
        $parts = [
            'scheme'    => $uri->getScheme(),
            'user_info' => $uri->getUserInfo(),
            'host'      => $uri->getHost(),
            'port'      => $uri->getPort(),
            'path'      => $uri->getPath(),
            'query'     => $uri->getQueryAsArray(),
            'fragment'  => $uri->getFragment(),
        ];
        $this->assertFalse($uri->isValid(), var_export($parts, 1));
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\ContentSecurityPolicy;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class ContentSecurityPolicyTest extends TestCase
{
    public function testContentSecurityPolicyFromStringThrowsExceptionIfImproperHeaderNameUsed()
    {
        $this->expectException(InvalidArgumentException::class);
        ContentSecurityPolicy::fromString('X-Content-Security-Policy: default-src *;');
    }

    public function testContentSecurityPolicyFromStringParsesDirectivesCorrectly()
    {
        $csp = ContentSecurityPolicy::fromString(
            "Content-Security-Policy: default-src 'none'; script-src 'self'; img-src 'self'; style-src 'self';"
        );
        $this->assertInstanceOf(HeaderInterface::class, $csp);
        $this->assertInstanceOf(ContentSecurityPolicy::class, $csp);
        $directives = [
            'default-src' => "'none'",
            'script-src'  => "'self'",
            'img-src'     => "'self'",
            'style-src'   => "'self'",
        ];
        $this->assertEquals($directives, $csp->getDirectives());
    }

    public function testContentSecurityPolicyGetFieldNameReturnsHeaderName()
    {
        $csp = new ContentSecurityPolicy();
        $this->assertEquals('Content-Security-Policy', $csp->getFieldName());
    }

    public function testContentSecurityPolicyToStringReturnsHeaderFormattedString()
    {
        $csp = ContentSecurityPolicy::fromString(
            "Content-Security-Policy: default-src 'none'; img-src 'self' https://*.gravatar.com;"
        );
        $this->assertInstanceOf(HeaderInterface::class, $csp);
        $this->assertInstanceOf(ContentSecurityPolicy::class, $csp);
        $this->assertEquals(
            "Content-Security-Policy: default-src 'none'; img-src 'self' https://*.gravatar.com;",
            $csp->toString()
        );
    }

    public function testContentSecurityPolicySetDirective()
    {
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('default-src', ['https://*.google.com', 'http://foo.com'])
            ->setDirective('img-src', ["'self'"])
            ->setDirective('script-src', ['https://*.googleapis.com', 'https://*.bar.com']);
        $header = 'Content-Security-Policy: default-src https://*.google.com http://foo.com; '
                . 'img-src \'self\'; script-src https://*.googleapis.com https://*.bar.com;';
        $this->assertEquals($header, $csp->toString());
    }

    public function testContentSecurityPolicySetDirectiveWithEmptySourcesDefaultsToNone()
    {
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('default-src', ["'self'"])
            ->setDirective('img-src', ['*'])
            ->setDirective('script-src', []);
        $this->assertEquals(
            "Content-Security-Policy: default-src 'self'; img-src *; script-src 'none';",
            $csp->toString()
        );
    }

    public function testContentSecurityPolicySetDirectiveThrowsExceptionIfInvalidDirectiveNameGiven()
    {
        $this->expectException(InvalidArgumentException::class);
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('foo', []);
    }

    public function testContentSecurityPolicyGetFieldValueReturnsProperValue()
    {
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('default-src', ["'self'"])
            ->setDirective('img-src', ['https://*.github.com']);
        $this->assertEquals("default-src 'self'; img-src https://*.github.com;", $csp->getFieldValue());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ContentSecurityPolicy::fromString("Content-Security-Policy: default-src 'none'\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaDirective()
    {
        $header = new ContentSecurityPolicy();
        $this->expectException(InvalidArgumentException::class);
        $header->setDirective('default-src', ["\rsome\r\nCRLF\ninjection"]);
    }

    public function testContentSecurityPolicySetDirectiveWithEmptyReportUriDefaultsToUnset()
    {
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('report-uri', []);
        $this->assertEquals(
            'Content-Security-Policy: ',
            $csp->toString()
        );
    }

    public function testContentSecurityPolicySetDirectiveWithEmptyReportUriRemovesExistingValue()
    {
        $csp = new ContentSecurityPolicy();
        $csp->setDirective('report-uri', ['csp-error']);
        $this->assertEquals(
            'Content-Security-Policy: report-uri csp-error;',
            $csp->toString()
        );

        $csp->setDirective('report-uri', []);
        $this->assertEquals(
            'Content-Security-Policy: ',
            $csp->toString()
        );
    }
}

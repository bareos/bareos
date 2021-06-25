<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\AcceptLanguage;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class AcceptLanguageTest extends TestCase
{
    public function testAcceptLanguageFromStringCreatesValidAcceptLanguageHeader()
    {
        $acceptLanguageHeader = AcceptLanguage::fromString('Accept-Language: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $acceptLanguageHeader);
        $this->assertInstanceOf(AcceptLanguage::class, $acceptLanguageHeader);
    }

    public function testAcceptLanguageGetFieldNameReturnsHeaderName()
    {
        $acceptLanguageHeader = new AcceptLanguage();
        $this->assertEquals('Accept-Language', $acceptLanguageHeader->getFieldName());
    }

    public function testAcceptLanguageGetFieldValueReturnsProperValue()
    {
        $acceptLanguageHeader = AcceptLanguage::fromString('Accept-Language: xxx');
        $this->assertEquals('xxx', $acceptLanguageHeader->getFieldValue());
    }

    public function testAcceptLanguageGetFieldValueReturnsProperValueWithTrailingSemicolon()
    {
        $acceptLanguageHeader = AcceptLanguage::fromString('Accept-Language: xxx;');
        $this->assertEquals('xxx', $acceptLanguageHeader->getFieldValue());
    }

    public function testAcceptLanguageGetFieldValueReturnsProperValueWithSemicolonWithoutEqualSign()
    {
        $acceptLanguageHeader = AcceptLanguage::fromString('Accept-Language: xxx;yyy');
        $this->assertEquals('xxx;yyy', $acceptLanguageHeader->getFieldValue());
    }

    public function testAcceptLanguageToStringReturnsHeaderFormattedString()
    {
        $acceptLanguageHeader = new AcceptLanguage();
        $acceptLanguageHeader->addLanguage('da', 0.8)
                             ->addLanguage('en-gb', 1);

        $this->assertEquals('Accept-Language: da;q=0.8, en-gb', $acceptLanguageHeader->toString());
    }

    /** Implementation specific tests here */

    public function testCanParseCommaSeparatedValues()
    {
        $header = AcceptLanguage::fromString('Accept-Language: da;q=0.8, en-gb');
        $this->assertTrue($header->hasLanguage('da'));
        $this->assertTrue($header->hasLanguage('en-gb'));
    }

    public function testPrioritizesValuesBasedOnQParameter()
    {
        $header   = AcceptLanguage::fromString('Accept-Language: da;q=0.8, en-gb, *;q=0.4');
        $expected = [
            'en-gb',
            'da',
            '*',
        ];

        $test = [];
        foreach ($header->getPrioritized() as $type) {
            $this->assertEquals(array_shift($expected), $type->typeString);
        }
        $this->assertEquals($expected, $test);
    }

    public function testWildcharLanguage()
    {
        $acceptHeader = new AcceptLanguage();
        $acceptHeader->addLanguage('da', 0.8)
                     ->addLanguage('*', 0.4);

        $this->assertTrue($acceptHeader->hasLanguage('da'));
        $this->assertTrue($acceptHeader->hasLanguage('en'));
        $this->assertEquals('Accept-Language: da;q=0.8, *;q=0.4', $acceptHeader->toString());
    }

    public function testWildcards()
    {
        $accept = AcceptLanguage::fromString('*, en-*, en-us');
        $res = $accept->getPrioritized();

        $this->assertEquals('en-us', $res[0]->getLanguage());
        $this->assertEquals('en', $res[0]->getPrimaryTag());
        $this->assertEquals('us', $res[0]->getSubTag());

        $this->assertEquals('en-*', $res[1]->getLanguage());
        $this->assertEquals('en', $res[1]->getPrimaryTag());

        $this->assertTrue($accept->hasLanguage('nl'));
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        $header = AcceptLanguage::fromString("Accept-Language: da\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaSetters()
    {
        $header = new AcceptLanguage();
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('valid type');

        $header->addLanguage("\nen\r-\r\nus");
    }
}

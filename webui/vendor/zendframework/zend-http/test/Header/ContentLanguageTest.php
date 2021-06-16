<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\ContentLanguage;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class ContentLanguageTest extends TestCase
{
    public function testContentLanguageFromStringCreatesValidContentLanguageHeader()
    {
        $contentLanguageHeader = ContentLanguage::fromString('Content-Language: xxx');
        $this->assertInstanceOf(HeaderInterface::class, $contentLanguageHeader);
        $this->assertInstanceOf(ContentLanguage::class, $contentLanguageHeader);
    }

    public function testContentLanguageGetFieldNameReturnsHeaderName()
    {
        $contentLanguageHeader = new ContentLanguage();
        $this->assertEquals('Content-Language', $contentLanguageHeader->getFieldName());
    }

    public function testContentLanguageGetFieldValueReturnsProperValue()
    {
        $this->markTestIncomplete('ContentLanguage needs to be completed');

        $contentLanguageHeader = new ContentLanguage();
        $this->assertEquals('xxx', $contentLanguageHeader->getFieldValue());
    }

    public function testContentLanguageToStringReturnsHeaderFormattedString()
    {
        $this->markTestIncomplete('ContentLanguage needs to be completed');

        $contentLanguageHeader = new ContentLanguage();

        // @todo set some values, then test output
        $this->assertEmpty('Content-Language: xxx', $contentLanguageHeader->toString());
    }

    /** Implementation specific tests here */

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        ContentLanguage::fromString("Content-Language: xxx\r\n\r\nevilContent");
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaConstructor()
    {
        $this->expectException(InvalidArgumentException::class);
        new ContentLanguage("xxx\r\n\r\nevilContent");
    }
}

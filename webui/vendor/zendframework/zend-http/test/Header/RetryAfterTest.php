<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;
use Zend\Http\Header\RetryAfter;

class RetryAfterTest extends TestCase
{
    public function testRetryAfterFromStringCreatesValidRetryAfterHeader()
    {
        $retryAfterHeader = RetryAfter::fromString('Retry-After: 10');
        $this->assertInstanceOf(HeaderInterface::class, $retryAfterHeader);
        $this->assertInstanceOf(RetryAfter::class, $retryAfterHeader);
        $this->assertEquals('10', $retryAfterHeader->getDeltaSeconds());
    }

    public function testRetryAfterFromStringCreatesValidRetryAfterHeaderFromDate()
    {
        $retryAfterHeader = RetryAfter::fromString('Retry-After: Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Sun, 06 Nov 1994 08:49:37 GMT', $retryAfterHeader->getDate());
    }

    public function testRetryAfterGetFieldNameReturnsHeaderName()
    {
        $retryAfterHeader = new RetryAfter();
        $this->assertEquals('Retry-After', $retryAfterHeader->getFieldName());
    }

    public function testRetryAfterGetFieldValueReturnsProperValue()
    {
        $retryAfterHeader = new RetryAfter();
        $retryAfterHeader->setDeltaSeconds(3600);
        $this->assertEquals('3600', $retryAfterHeader->getFieldValue());
        $retryAfterHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Sun, 06 Nov 1994 08:49:37 GMT', $retryAfterHeader->getFieldValue());
    }

    public function testRetryAfterToStringReturnsHeaderFormattedString()
    {
        $retryAfterHeader = new RetryAfter();

        $retryAfterHeader->setDeltaSeconds(3600);
        $this->assertEquals('Retry-After: 3600', $retryAfterHeader->toString());

        $retryAfterHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Retry-After: Sun, 06 Nov 1994 08:49:37 GMT', $retryAfterHeader->toString());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        RetryAfter::fromString("Retry-After: 10\r\n\r\nevilContent");
    }
}

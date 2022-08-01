<?php
/**
 * @see       https://github.com/zendframework/zend-http for the canonical source repository
 * @copyright Copyright (c) 2005-2017 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   https://github.com/zendframework/zend-http/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Http\Header;

use DateTime;
use DateTimeZone;
use PHPUnit\Framework\TestCase;
use Zend\Http\Header\Date;
use Zend\Http\Header\Exception\InvalidArgumentException;
use Zend\Http\Header\HeaderInterface;

class DateTest extends TestCase
{
    public function tearDown()
    {
        // set to RFC default date format
        Date::setDateFormat(Date::DATE_RFC1123);
    }

    public function testDateFromStringCreatesValidDateHeader()
    {
        $dateHeader = Date::fromString('Date: Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertInstanceOf(HeaderInterface::class, $dateHeader);
        $this->assertInstanceOf(Date::class, $dateHeader);
    }

    public function testDateFromTimeStringCreatesValidDateHeader()
    {
        $dateHeader = Date::fromTimeString('+12 hours');

        $this->assertInstanceOf(HeaderInterface::class, $dateHeader);
        $this->assertInstanceOf(Date::class, $dateHeader);

        $date     = new DateTime(null, new DateTimeZone('GMT'));
        $interval = $dateHeader->date()->diff($date, 1);

        if (PHP_VERSION_ID >= 70200) {
            $this->assertSame('+11 hours 59 minutes 59 seconds', $interval->format('%R%H hours %I minutes %S seconds'));
            $this->assertLessThan(1, $interval->f);
            $this->assertGreaterThan(0, $interval->f);
        } else {
            $this->assertSame('+12 hours 00 minutes 00 seconds', $interval->format('%R%H hours %I minutes %S seconds'));
        }
    }

    public function testDateFromTimestampCreatesValidDateHeader()
    {
        $dateHeader = Date::fromTimestamp(time() + 12 * 60 * 60);

        $this->assertInstanceOf(HeaderInterface::class, $dateHeader);
        $this->assertInstanceOf(Date::class, $dateHeader);

        $date     = new DateTime(null, new DateTimeZone('GMT'));
        $interval = $dateHeader->date()->diff($date, 1);

        if (PHP_VERSION_ID >= 70200) {
            $this->assertSame('+11 hours 59 minutes 59 seconds', $interval->format('%R%H hours %I minutes %S seconds'));
            $this->assertLessThan(1, $interval->f);
            $this->assertGreaterThan(0, $interval->f);
        } else {
            $this->assertSame('+12 hours 00 minutes 00 seconds', $interval->format('%R%H hours %I minutes %S seconds'));
        }
    }

    public function testDateFromTimeStringDetectsBadInput()
    {
        $this->expectException(InvalidArgumentException::class);
        Date::fromTimeString('3 Days of the Condor');
    }

    public function testDateFromTimestampDetectsBadInput()
    {
        $this->expectException(InvalidArgumentException::class);
        Date::fromTimestamp('The Day of the Jackal');
    }

    public function testDateGetFieldNameReturnsHeaderName()
    {
        $dateHeader = new Date();
        $this->assertEquals('Date', $dateHeader->getFieldName());
    }

    public function testDateGetFieldValueReturnsProperValue()
    {
        $dateHeader = new Date();
        $dateHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Sun, 06 Nov 1994 08:49:37 GMT', $dateHeader->getFieldValue());
    }

    public function testDateToStringReturnsHeaderFormattedString()
    {
        $dateHeader = new Date();
        $dateHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertEquals('Date: Sun, 06 Nov 1994 08:49:37 GMT', $dateHeader->toString());
    }

    /** Implementation specific tests here */

    public function testDateReturnsDateTimeObject()
    {
        $dateHeader = new Date();
        $this->assertInstanceOf(DateTime::class, $dateHeader->date());
    }

    public function testDateFromStringCreatesValidDateTime()
    {
        $dateHeader = Date::fromString('Date: Sun, 06 Nov 1994 08:49:37 GMT');
        $this->assertInstanceOf(DateTime::class, $dateHeader->date());
        $this->assertEquals('Sun, 06 Nov 1994 08:49:37 GMT', $dateHeader->date()->format('D, d M Y H:i:s \G\M\T'));
    }

    public function testDateReturnsProperlyFormattedDate()
    {
        $date = new DateTime('now', new DateTimeZone('GMT'));

        $dateHeader = new Date();
        $dateHeader->setDate($date);
        $this->assertEquals($date->format('D, d M Y H:i:s \G\M\T'), $dateHeader->getDate());
    }

    public function testDateThrowsExceptionForInvalidDate()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid date');
        $dateHeader = new Date();
        $dateHeader->setDate('~~~~');
    }

    public function testDateCanCompareDates()
    {
        $dateHeader = new Date();
        $dateHeader->setDate('1 day ago');
        $this->assertEquals(-1, $dateHeader->compareTo(new DateTime('now')));
    }

    public function testDateCanOutputDatesInOldFormats()
    {
        Date::setDateFormat(Date::DATE_ANSIC);

        $dateHeader = new Date();
        $dateHeader->setDate('Sun, 06 Nov 1994 08:49:37 GMT');

        $this->assertEquals('Date: Sun Nov 6 08:49:37 1994', $dateHeader->toString());
    }

    /**
     * @see http://en.wikipedia.org/wiki/HTTP_response_splitting
     * @group ZF2015-04
     */
    public function testPreventsCRLFAttackViaFromString()
    {
        $this->expectException(InvalidArgumentException::class);
        Date::fromString("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\nevilContent");
    }
}

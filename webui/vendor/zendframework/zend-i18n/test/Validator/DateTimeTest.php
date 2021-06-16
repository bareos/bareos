<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Validator;

use DateTime;
use IntlDateFormatter;
use Locale;
use PHPUnit\Framework\TestCase;
use Zend\I18n\Validator\DateTime as DateTimeValidator;

class DateTimeTest extends TestCase
{
    /**
     * @var DateTimeValidator
     */
    protected $validator;

    /**
     * @var Locale
     */
    protected $locale;

    /**
     * @var \DateTimeZone
     */
    protected $timezone;

    protected function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->locale = Locale::getDefault();
        $this->timezone = date_default_timezone_get();

        $this->validator = new DateTimeValidator([
            'locale' => 'en',
            'timezone' => 'Europe/Amsterdam'
        ]);
    }

    protected function tearDown()
    {
        if (extension_loaded('intl')) {
            Locale::setDefault($this->locale);
        }
        date_default_timezone_set($this->timezone);
    }

    /**
     * Ensures that the validator follows expected behavior
     *
     * @dataProvider basicProvider name of method that provides parameters
     * @param string  $value    that will be tested
     * @param boolean $expected expected result of assertion
     * @param array   $options  fed into the validator before validation
     */
    public function testBasic($value, $expected, $options = [])
    {
        $this->validator->setOptions($options);

        $this->assertEquals(
            $expected,
            $this->validator->isValid($value),
            sprintf('Failed expecting %s being %s', $value, ($expected ? 'true' : 'false'))
                . sprintf(
                    ' (locale:%s, dateType: %s, timeType: %s, pattern:%s)',
                    $this->validator->getLocale(),
                    $this->validator->getDateType(),
                    $this->validator->getTimeType(),
                    $this->validator->getPattern()
                )
        );
    }

    public function basicProvider()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $trueArray      = [];
        $testingDate    = new DateTime();
        $testingLocales = ['en', 'de', 'zh-TW', 'ja', 'ar', 'ru', 'si', 'ml-IN', 'hi'];
        $testingFormats = [
            IntlDateFormatter::FULL,
            IntlDateFormatter::LONG,
            IntlDateFormatter::MEDIUM,
            IntlDateFormatter::SHORT,
            IntlDateFormatter::NONE
        ];

        //Loop locales and formats for a more thorough set of "true" test data
        foreach ($testingLocales as $locale) {
            foreach ($testingFormats as $dateFormat) {
                foreach ($testingFormats as $timeFormat) {
                    if (($timeFormat !== IntlDateFormatter::NONE) || ($dateFormat !== IntlDateFormatter::NONE)) {
                        $trueArray[] = [
                            IntlDateFormatter::create($locale, $dateFormat, $timeFormat)->format($testingDate),
                            true,
                            ['locale' => $locale, 'dateType' => $dateFormat, 'timeType' => $timeFormat]
                        ];
                    }
                }
            }
        }

        $falseArray = [
            [
                'May 38, 2013',
                false,
                [
                    'locale' => 'en',
                    'dateType' => IntlDateFormatter::FULL,
                    'timeType' => IntlDateFormatter::NONE
                ]
            ]
        ];

        return array_merge($trueArray, $falseArray);
    }

    /**
     * Ensures that getMessages() returns expected default value
     *
     * @return void
     */
    public function testGetMessages()
    {
        $this->assertEquals([], $this->validator->getMessages());
    }

    /**
     * Ensures that set/getLocale() works
     */
    public function testOptionLocale()
    {
        $this->validator->setLocale('de');
        $this->assertEquals('de', $this->validator->getLocale());
    }

    public function testApplicationOptionLocale()
    {
        Locale::setDefault('nl');
        $valid = new DateTimeValidator();
        $this->assertEquals(Locale::getDefault(), $valid->getLocale());
    }

    /**
     * Ensures that set/getTimezone() works
     */
    public function testOptionTimezone()
    {
        $this->validator->setLocale('Europe/Berlin');
        $this->assertEquals('Europe/Berlin', $this->validator->getLocale());
    }

    public function testApplicationOptionTimezone()
    {
        date_default_timezone_set('Europe/Berlin');
        $valid = new DateTimeValidator();
        $this->assertEquals(date_default_timezone_get(), $valid->getTimezone());
    }

    /**
     * Ensures that an omitted pattern results in a calculated pattern by IntlDateFormatter
     */
    public function testOptionPatternOmitted()
    {
        // null before validation
        $this->assertNull($this->validator->getPattern());

        $this->validator->isValid('does not matter');

        // set after
        $this->assertEquals('yyyyMMdd hh:mm a', $this->validator->getPattern());
    }

    /**
     * Ensures that setting the pattern results in pattern used (by the validation process)
     */
    public function testOptionPattern()
    {
        $this->validator->setOptions(['pattern' => 'hh:mm']);

        $this->assertTrue($this->validator->isValid('02:00'));
        $this->assertEquals('hh:mm', $this->validator->getPattern());
    }

    public function testMultipleIsValidCalls()
    {
        $validValue = IntlDateFormatter::create('en', IntlDateFormatter::FULL, IntlDateFormatter::FULL)
            ->format(new DateTime());
        $this->validator
            ->setLocale('en')
            ->setDateType(IntlDateFormatter::FULL)
            ->setTimeType(IntlDateFormatter::FULL);

        $this->assertTrue($this->validator->isValid($validValue));
        $this->assertFalse($this->validator->isValid('12/31/2015'));
        $this->assertFalse($this->validator->isValid('23:59:59'));
        $this->assertFalse($this->validator->isValid('does not matter'));
        $this->assertTrue($this->validator->isValid($validValue));
    }
}

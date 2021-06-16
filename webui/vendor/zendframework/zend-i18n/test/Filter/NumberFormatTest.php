<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Filter;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Filter\NumberFormat as NumberFormatFilter;
use NumberFormatter;

class NumberFormatTest extends TestCase
{
    protected function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }
    }

    public function testConstructWithOptions()
    {
        $filter = new NumberFormatFilter([
            'locale' => 'en_US',
            'style'  => NumberFormatter::DECIMAL
        ]);

        $this->assertEquals('en_US', $filter->getLocale());
        $this->assertEquals(NumberFormatter::DECIMAL, $filter->getStyle());
    }

    public function testConstructWithParameters()
    {
        $filter = new NumberFormatFilter('en_US', NumberFormatter::DECIMAL);

        $this->assertEquals('en_US', $filter->getLocale());
        $this->assertEquals(NumberFormatter::DECIMAL, $filter->getStyle());
    }


    /**
     * @param $locale
     * @param $style
     * @param $type
     * @param $value
     * @param $expected
     * @dataProvider numberToFormattedProvider
     */
    public function testNumberToFormatted($locale, $style, $type, $value, $expected)
    {
        $filter = new NumberFormatFilter($locale, $style, $type);
        $this->assertEquals($expected, $filter->filter($value));
    }

    /**
     * @param $locale
     * @param $style
     * @param $type
     * @param $value
     * @param $expected
     * @dataProvider formattedToNumberProvider
     */
    public function testFormattedToNumber($locale, $style, $type, $value, $expected)
    {
        $filter = new NumberFormatFilter($locale, $style, $type);
        $this->assertEquals($expected, $filter->filter($value));
    }

    public function numberToFormattedProvider()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        return [
            [
                'en_US',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                1234567.8912346,
                '1,234,567.891'
            ],
            [
                'de_DE',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                1234567.8912346,
                '1.234.567,891'
            ],
            [
                'ru_RU',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                1234567.8912346,
                '1 234 567,891'
            ],
        ];
    }

    public function formattedToNumberProvider()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        return [
            [
                'en_US',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                '1,234,567.891',
                1234567.891,
            ],
            [
                'de_DE',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                '1.234.567,891',
                1234567.891,
            ],
            [
                'ru_RU',
                NumberFormatter::DEFAULT_STYLE,
                NumberFormatter::TYPE_DOUBLE,
                '1 234 567,891',
                1234567.891,
            ],
        ];
    }


    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                '1.234.567,891',
                '1.567,891'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new NumberFormatFilter('de_AT', NumberFormatter::DEFAULT_STYLE, NumberFormatter::TYPE_DOUBLE);

        $this->assertEquals($input, $filter->filter($input));
    }
}

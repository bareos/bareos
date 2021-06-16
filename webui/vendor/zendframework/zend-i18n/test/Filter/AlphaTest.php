<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Filter;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Filter\Alpha as AlphaFilter;
use Locale;

/**
 * @group      Zend_Filter
 */
class AlphaTest extends TestCase
{
    /**
     * AlphaFilter object
     *
     * @var AlphaFilter
     */
    protected $filter;

    /**
     * Is PCRE is compiled with UTF-8 and Unicode support
     *
     * @var mixed
     **/
    protected static $unicodeEnabled;

    /**
     * Locale in browser.
     *
     * @var string
     */
    protected $locale;

    /**
     * The Alphabet means english alphabet.
     *
     * @var bool
     */
    protected static $meansEnglishAlphabet;

    /**
     * Creates a new AlnumFilter object for each test method
     *
     * @return void
     */
    protected function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->filter = new AlphaFilter();

        $this->locale               = Locale::getDefault();
        $language                   = Locale::getPrimaryLanguage($this->locale);
        self::$meansEnglishAlphabet = $language === 'ja';
        self::$unicodeEnabled       = (bool) @preg_match('/\pL/u', 'a');
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        if (! self::$unicodeEnabled) {
            // POSIX named classes are not supported, use alternative a-zA-Z match
            $valuesExpected = [
                'abc123'        => 'abc',
                'abc 123'       => 'abc',
                'abcxyz'        => 'abcxyz',
                ''              => ''
            ];
        } elseif (self::$meansEnglishAlphabet) {
            //The Alphabet means english alphabet.
            /**
             * The first element contains multibyte alphabets.
             *  But , AlphaFilter is expected to return only singlebyte alphabets.
             * The second contains multibyte or singlebyte space.
             * The third  contains multibyte or singlebyte digits.
             * The forth  contains various multibyte or singlebyte characters.
             * The last contains only singlebyte alphabets.
             */
            $valuesExpected = [
                'aＡBｂc'       => 'aBc',
                'z Ｙ　x'       => 'zx',
                'Ｗ1v３Ｕ4t'    => 'vt',
                '，sй.rλ:qν＿p' => 'srqp',
                'onml'          => 'onml'
            ];
        } else {
            //The Alphabet means each language's alphabet.
            $valuesExpected = [
                'abc123'        => 'abc',
                'abc 123'       => 'abc',
                'abcxyz'        => 'abcxyz',
                'četně'         => 'četně',
                'لعربية'        => 'لعربية',
                'grzegżółka'    => 'grzegżółka',
                'België'        => 'België',
                ''              => ''
            ];
        }

        foreach ($valuesExpected as $input => $expected) {
            $actual = $this->filter->filter($input);
            $this->assertEquals($expected, $actual);
        }
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testAllowWhiteSpace()
    {
        $this->filter->setAllowWhiteSpace(true);

        if (! self::$unicodeEnabled) {
            // POSIX named classes are not supported, use alternative a-zA-Z match
            $valuesExpected = [
                'abc123'   => 'abc',
                'abc 123'  => 'abc ',
                'abcxyz'   => 'abcxyz',
                ''         => '',
                "\n"       => "\n",
                " \t "     => " \t "
            ];
        }
        if (self::$meansEnglishAlphabet) {
            //The Alphabet means english alphabet.
            $valuesExpected = [
                'a B'    => 'a B',
                'zＹ　x' => 'zx'
            ];
        } else {
            //The Alphabet means each language's alphabet.
            $valuesExpected = [
                'abc123'        => 'abc',
                'abc 123'       => 'abc ',
                'abcxyz'        => 'abcxyz',
                'četně'         => 'četně',
                'لعربية'        => 'لعربية',
                'grzegżółka'    => 'grzegżółka',
                'België'        => 'België',
                ''              => '',
                "\n"            => "\n",
                " \t "          => " \t "
                ];
        }

        foreach ($valuesExpected as $input => $expected) {
            $actual = $this->filter->filter($input);
            $this->assertEquals($expected, $actual);
        }
    }

    public function testFilterSupportArray()
    {
        $filter = new AlphaFilter();

        $values = [
            'abc123'        => 'abc',
            'abc 123'       => 'abc',
            'abcxyz'        => 'abcxyz',
            ''              => ''
        ];

        $actual = $filter->filter(array_keys($values));

        $this->assertEquals(array_values($values), $actual);
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new AlphaFilter();

        $this->assertEquals($input, $filter->filter($input));
    }
}

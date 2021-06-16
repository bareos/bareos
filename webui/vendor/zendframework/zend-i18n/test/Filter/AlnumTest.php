<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Filter;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Filter\Alnum as AlnumFilter;
use Locale;

/**
 * @group      Zend_Filter
 */
class AlnumTest extends TestCase
{
    /**
     * AlnumFilter object
     *
     * @var AlnumFilter
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
     * @var string object
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

        $this->filter = new AlnumFilter();

        $this->locale               = Locale::getDefault();
        $language                   = Locale::getPrimaryLanguage($this->locale);
        static::$meansEnglishAlphabet = $language === 'ja';
        static::$unicodeEnabled       = (bool) @preg_match('/\pL/u', 'a');
    }

    /**
     * Ensures that the filter follows expected behavior
     *
     * @return void
     */
    public function testBasic()
    {
        if (! static::$unicodeEnabled) {
            // POSIX named classes are not supported, use alternative a-zA-Z match
            $valuesExpected = [
                'abc123'  => 'abc123',
                'abc 123' => 'abc123',
                'abcxyz'  => 'abcxyz',
                'AZ@#4.3' => 'AZ43',
                ''        => ''
            ];
        } elseif (static::$meansEnglishAlphabet) {
            // The Alphabet means english alphabet.

            /**
             * The first element contains multibyte alphabets and digits.
             *  But , AlnumFilter is expected to return only singlebyte alphabets and digits.
             *
             * The second contains multibyte or singebyte space.
             * The third  contains various multibyte or singebyte characters.
             */
            $valuesExpected = [
                'aＡBｂ3４5６'  => 'aB35',
                'z７ Ｙ8　x９'  => 'z8x',
                '，s1.2r３#:q,' => 's12rq',
            ];
        } else {
            //The Alphabet means each language's alphabet.
            $valuesExpected = [
                'abc123'        => 'abc123',
                'abc 123'       => 'abc123',
                'abcxyz'        => 'abcxyz',
                'če2t3ně'       => 'če2t3ně',
                'grz5e4gżółka'  => 'grz5e4gżółka',
                'Be3l5gië'      => 'Be3l5gië',
                ''              => ''
            ];
        }

        foreach ($valuesExpected as $input => $expected) {
            $actual = $this->filter->filter($input);
            $this->assertEquals($expected, $actual);
        }
    }

    /**
     * Ensures that the allowWhiteSpace option works as expected
     *
     * @return void
     */
    public function testAllowWhiteSpace()
    {
        $this->filter->setAllowWhiteSpace(true);

        if (! static::$unicodeEnabled) {
            // POSIX named classes are not supported, use alternative a-zA-Z match
            $valuesExpected = [
                'abc123'  => 'abc123',
                'abc 123' => 'abc 123',
                'abcxyz'  => 'abcxyz',
                'AZ@#4.3' => 'AZ43',
                ''        => '',
                "\n"      => "\n",
                " \t "    => " \t "
            ];
        } elseif (static::$meansEnglishAlphabet) {
            //The Alphabet means english alphabet.
            $valuesExpected = [
                'a B ４5' => 'a B 5',
                'z3　x'   => 'z3x'
            ];
        } else {
            //The Alphabet means each language's alphabet.
            $valuesExpected = [
                'abc123'        => 'abc123',
                'abc 123'       => 'abc 123',
                'abcxyz'        => 'abcxyz',
                'če2 t3ně'      => 'če2 t3ně',
                'gr z5e4gżółka' => 'gr z5e4gżółka',
                'Be3l5 gië'     => 'Be3l5 gië',
                ''              => '',
            ];
        }

        foreach ($valuesExpected as $input => $expected) {
            $actual = $this->filter->filter($input);
            $this->assertEquals($expected, $actual);
        }
    }

    public function testFilterSupportArray()
    {
        $filter = new AlnumFilter();

        $values = [
            'abc123'  => 'abc123',
            'abc 123' => 'abc123',
            'abcxyz'  => 'abcxyz',
            'AZ@#4.3' => 'AZ43',
            ''        => ''
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
        $filter = new AlnumFilter();

        $this->assertEquals($input, $filter->filter($input));
    }
}

<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter\Word;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Word\SeparatorToCamelCase as SeparatorToCamelCaseFilter;

class SeparatorToCamelCaseTest extends TestCase
{
    public function testFilterSeparatesCamelCasedWordsWithSpacesByDefault()
    {
        $string   = 'camel cased words';
        $filter   = new SeparatorToCamelCaseFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('CamelCasedWords', $filtered);
    }

    public function testFilterSeparatesCamelCasedWordsWithProvidedSeparator()
    {
        $string   = 'camel:-:cased:-:Words';
        $filter   = new SeparatorToCamelCaseFilter(':-:');
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('CamelCasedWords', $filtered);
    }

    /**
     * @group ZF-10517
     */
    public function testFilterSeparatesUniCodeCamelCasedWordsWithProvidedSeparator()
    {
        if (! extension_loaded('mbstring')) {
            $this->markTestSkipped('Extension mbstring not available');
        }

        $string   = 'camel:-:cased:-:Words';
        $filter   = new SeparatorToCamelCaseFilter(':-:');
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('CamelCasedWords', $filtered);
    }

    /**
     * @group ZF-10517
     */
    public function testFilterSeparatesUniCodeCamelCasedUserWordsWithProvidedSeparator()
    {
        if (! extension_loaded('mbstring')) {
            $this->markTestSkipped('Extension mbstring not available');
        }

        $string   = 'test šuma';
        $filter   = new SeparatorToCamelCaseFilter(' ');
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('TestŠuma', $filtered);
    }

    /**
     * @group 6151
     */
    public function testFilterSeparatesCamelCasedNonAlphaWordsWithProvidedSeparator()
    {
        $string   = 'user_2_user';
        $filter   = new SeparatorToCamelCaseFilter('_');
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('User2User', $filtered);
    }

    /**
     * @return void
     */
    public function testFilterSupportArray()
    {
        $filter = new SeparatorToCamelCaseFilter();

        $input = [
            'camel cased words',
            'something different'
        ];

        $filtered = $filter($input);

        $this->assertNotEquals($input, $filtered);
        $this->assertEquals(['CamelCasedWords', 'SomethingDifferent'], $filtered);
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
        $filter = new SeparatorToCamelCaseFilter();

        $this->assertEquals($input, $filter($input));
    }
}

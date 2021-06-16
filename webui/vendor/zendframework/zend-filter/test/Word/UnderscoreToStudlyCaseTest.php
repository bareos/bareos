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
use Zend\Filter\Word\UnderscoreToStudlyCase;

class UnderscoreToStudlyCaseTest extends TestCase
{
    public function testFilterSeparatesStudlyCasedWordsWithDashes()
    {
        $string   = 'studly_cased_words';
        $filter   = new UnderscoreToStudlyCase();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('studlyCasedWords', $filtered);
    }

    public function testSomeFilterValues()
    {
        $filter   = new UnderscoreToStudlyCase();

        $string   = 'zend_framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('zendFramework', $filtered);

        $string   = 'zend_Framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('zendFramework', $filtered);

        $string   = 'zendFramework';
        $filtered = $filter($string);
        $this->assertEquals('zendFramework', $filtered);

        $string   = 'zendframework';
        $filtered = $filter($string);
        $this->assertEquals('zendframework', $filtered);

        $string   = '_zendframework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('zendframework', $filtered);

        $string   = '_zend_framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('zendFramework', $filtered);
    }

    public function testFiltersArray()
    {
        $filter   = new UnderscoreToStudlyCase();

        $string   = ['zend_framework', '_zend_framework'];
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals(['zendFramework', 'zendFramework'], $filtered);
    }

    public function testWithEmpties()
    {
        $filter   = new UnderscoreToStudlyCase();

        $string   = '';
        $filtered = $filter($string);
        $this->assertEquals('', $filtered);

        $string   = ['', 'Zend_Framework'];
        $filtered = $filter($string);
        $this->assertEquals(['', 'zendFramework'], $filtered);
    }
}

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
use Zend\Filter\Word\UnderscoreToCamelCase as UnderscoreToCamelCaseFilter;

class UnderscoreToCamelCaseTest extends TestCase
{
    public function testFilterSeparatesCamelCasedWordsWithDashes()
    {
        $string   = 'camel_cased_words';
        $filter   = new UnderscoreToCamelCaseFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('CamelCasedWords', $filtered);
    }

    /**
     * ZF-4097
     */
    public function testSomeFilterValues()
    {
        $filter   = new UnderscoreToCamelCaseFilter();

        $string   = 'zend_framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('ZendFramework', $filtered);

        $string   = 'zend_Framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('ZendFramework', $filtered);

        $string   = 'zendFramework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('ZendFramework', $filtered);

        $string   = 'zendframework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Zendframework', $filtered);

        $string   = '_zendframework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Zendframework', $filtered);

        $string   = '_zend_framework';
        $filtered = $filter($string);
        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('ZendFramework', $filtered);
    }
}

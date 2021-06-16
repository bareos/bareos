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
use Zend\Filter\Word\CamelCaseToUnderscore as CamelCaseToUnderscoreFilter;

class CamelCaseToUnderscoreTest extends TestCase
{
    public function testFilterSeparatesCamelCasedWordsWithUnderscores()
    {
        $string   = 'CamelCasedWords';
        $filter   = new CamelCaseToUnderscoreFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Camel_Cased_Words', $filtered);
    }

    public function testFilterSeperatingNumbersToUnterscore()
    {
        $string = 'PaTitle';
        $filter   = new CamelCaseToUnderscoreFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Pa_Title', $filtered);

        $string = 'Pa2Title';
        $filter   = new CamelCaseToUnderscoreFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Pa2_Title', $filtered);

        $string = 'Pa2aTitle';
        $filter   = new CamelCaseToUnderscoreFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('Pa2a_Title', $filtered);
    }
}

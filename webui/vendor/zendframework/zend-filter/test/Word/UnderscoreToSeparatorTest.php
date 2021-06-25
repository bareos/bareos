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
use Zend\Filter\Word\UnderscoreToSeparator as UnderscoreToSeparatorFilter;

class UnderscoreToSeparatorTest extends TestCase
{
    public function testFilterSeparatesCamelCasedWordsDefaultSeparator()
    {
        $string   = 'underscore_separated_words';
        $filter   = new UnderscoreToSeparatorFilter();
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('underscore separated words', $filtered);
    }

    public function testFilterSeparatesCamelCasedWordsProvidedSeparator()
    {
        $string   = 'underscore_separated_words';
        $filter   = new UnderscoreToSeparatorFilter(':=:');
        $filtered = $filter($string);

        $this->assertNotEquals($string, $filtered);
        $this->assertEquals('underscore:=:separated:=:words', $filtered);
    }
}

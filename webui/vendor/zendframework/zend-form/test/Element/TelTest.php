<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Element;

use PHPUnit\Framework\TestCase;
use Zend\Filter\StringTrim;
use Zend\Filter\StripNewlines;
use Zend\Form\Element\Tel;
use Zend\Validator\Regex;

class TelTest extends TestCase
{
    public function testType()
    {
        $element = new Tel('test');

        $this->assertSame('tel', $element->getAttribute('type'));
    }

    public function testInputSpecification()
    {
        $name = 'test';
        $element = new Tel($name);

        $inputSpec = $element->getInputSpecification();

        $this->assertSame($name, $inputSpec['name']);
        $this->assertTrue($inputSpec['required']);
        $expectedFilters = [StringTrim::class, StripNewlines::class];
        $this->assertInputSpecContainsFilters($expectedFilters, $inputSpec);
        $this->assertInputSpecContainsRegexValidator($inputSpec);
    }

    private function getFilterName(array $filterSpec)
    {
        return $filterSpec['name'];
    }

    /**
     * @param string[] $expectedFilters
     * @param array $inputSpec
     */
    private function assertInputSpecContainsFilters(array $expectedFilters, array $inputSpec)
    {
        $actualFilters = array_map([$this, 'getFilterName'], $inputSpec['filters']);
        $missingFilters = array_diff($expectedFilters, $actualFilters);
        $this->assertCount(0, $missingFilters);
    }

    /**
     * @param array $inputSpec
     */
    private function assertInputSpecContainsRegexValidator(array $inputSpec)
    {
        $regexValidatorFound = false;
        foreach ($inputSpec['validators'] as $validator) {
            if ($validator instanceof Regex && $validator->getPattern() === "/^[^\r\n]*$/") {
                $regexValidatorFound = true;
            }
        }
        $this->assertTrue($regexValidatorFound);
    }
}

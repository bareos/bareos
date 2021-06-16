<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Element;

use PHPUnit\Framework\TestCase;
use Zend\Form\Element\Week as WeekElement;

class WeekTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new WeekElement('foo');
        $element->setAttributes([
            'inclusive' => true,
            'min'       => '1970-W01',
            'max'       => '1970-W03',
            'step'      => '1',
        ]);

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);

        $expectedClasses = [
            'Zend\Validator\Regex',
            'Zend\Validator\GreaterThan',
            'Zend\Validator\LessThan',
            'Zend\Validator\DateStep',
        ];
        foreach ($inputSpec['validators'] as $validator) {
            $class = get_class($validator);
            $this->assertContains($class, $expectedClasses, $class);
            switch ($class) {
                case 'Zend\Validator\GreaterThan':
                    $this->assertTrue($validator->getInclusive());
                    $this->assertEquals('1970-W01', $validator->getMin());
                    break;
                case 'Zend\Validator\LessThan':
                    $this->assertTrue($validator->getInclusive());
                    $this->assertEquals('1970-W03', $validator->getMax());
                    break;
                case 'Zend\Validator\DateStep':
                    $dateInterval = new \DateInterval('P1W');
                    $this->assertEquals($dateInterval, $validator->getStep());
                    break;
                default:
                    break;
            }
        }
    }

    public function weekValuesDataProvider()
    {
        return [
            //    value        expected
            ['2012-W01',  true],
            ['2012-W52',  true],
            ['2012-01',   false],
            ['W12-2012',  false],
            ['2012-W1',   false],
            ['12-W01',    false],
        ];
    }

    /**
     * @dataProvider weekValuesDataProvider
     */
    public function testHTML5WeekValidation($value, $expected)
    {
        $element = new WeekElement('foo');
        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $weekValidator = $inputSpec['validators'][0];
        $this->assertEquals($expected, $weekValidator->isValid($value));
    }
}

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
use Zend\Form\Element\DateTimeLocal as DateTimeLocalElement;
use Zend\Form\Exception\InvalidArgumentException;

class DateTimeLocalTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new DateTimeLocalElement('foo');
        $element->setAttributes([
            'inclusive' => true,
            'min'       => '2000-01-01T00:00',
            'max'       => '2001-01-01T00:00',
            'step'      => '1',
        ]);

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);

        $expectedClasses = [
            'Zend\Validator\Date',
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
                    $this->assertEquals('2000-01-01T00:00', $validator->getMin());
                    break;
                case 'Zend\Validator\LessThan':
                    $this->assertTrue($validator->getInclusive());
                    $this->assertEquals('2001-01-01T00:00', $validator->getMax());
                    break;
                case 'Zend\Validator\DateStep':
                    $dateInterval = new \DateInterval('PT1M');
                    $this->assertEquals($dateInterval, $validator->getStep());
                    break;
                default:
                    break;
            }
        }
    }

    public function testFailsWithInvalidMinSpecification()
    {
        $element = new DateTimeLocalElement('foo');
        $element->setAttributes(
            [
            'inclusive' => true,
            'min'       => '2001-01-01T00:00Z',
            'step'      => '1',
            ]
        );

        $this->expectException(InvalidArgumentException::class);
        $element->getInputSpecification();
    }

    public function testFailsWithInvalidMaxSpecification()
    {
        $element = new DateTimeLocalElement('foo');
        $element->setAttributes(
            [
            'inclusive' => true,
            'max'       => '2001-01-01T00:00Z',
            'step'      => '1',
            ]
        );
        $this->expectException(InvalidArgumentException::class);
        $element->getInputSpecification();
    }
}

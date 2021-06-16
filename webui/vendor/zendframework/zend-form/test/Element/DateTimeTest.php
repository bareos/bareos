<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Element;

use DateTime;
use PHPUnit\Framework\TestCase;
use Zend\Form\Element\DateTime as DateTimeElement;
use Zend\Form\Exception\InvalidArgumentException;

class DateTimeTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new DateTimeElement('foo');
        $element->setAttributes([
            'inclusive' => true,
            'min'       => '2000-01-01T00:00Z',
            'max'       => '2001-01-01T00:00Z',
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
                    $this->assertEquals('2000-01-01T00:00Z', $validator->getMin());
                    break;
                case 'Zend\Validator\LessThan':
                    $this->assertTrue($validator->getInclusive());
                    $this->assertEquals('2001-01-01T00:00Z', $validator->getMax());
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

    public function testProvidesInputSpecificationThatIncludesDateTimeFormatterBasedOnAttributes()
    {
        $element = new DateTimeElement('foo');
        $element->setFormat(DateTime::W3C);

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('filters', $inputSpec);
        $this->assertInternalType('array', $inputSpec['filters']);

        foreach ($inputSpec['filters'] as $filter) {
            switch ($filter['name']) {
                case 'Zend\Filter\DateTimeFormatter':
                    $this->assertEquals($filter['options']['format'], DateTime::W3C);
                    $this->assertEquals($filter['options']['format'], $element->getFormat());
                    break;
                default:
                    break;
            }
        }
    }

    public function testUsesBrowserFormatByDefault()
    {
        $element = new DateTimeElement('foo');
        $this->assertEquals(DateTimeElement::DATETIME_FORMAT, $element->getFormat());
    }

    public function testSpecifyingADateTimeValueWillReturnBrowserFormattedStringByDefault()
    {
        $date = new DateTime();
        $element = new DateTimeElement('foo');
        $element->setValue($date);
        $this->assertEquals($date->format(DateTimeElement::DATETIME_FORMAT), $element->getValue());
    }

    public function testValueIsFormattedAccordingToFormatInElement()
    {
        $date = new DateTime();
        $element = new DateTimeElement('foo');
        $element->setFormat($date::RFC2822);
        $element->setValue($date);
        $this->assertEquals($date->format($date::RFC2822), $element->getValue());
    }

    public function testCanRetrieveDateTimeObjectByPassingBooleanFalseToGetValue()
    {
        $date = new DateTime();
        $element = new DateTimeElement('foo');
        $element->setValue($date);
        $this->assertSame($date, $element->getValue(false));
    }

    public function testSetFormatWithOptions()
    {
        $format = 'Y-m-d';
        $element = new DateTimeElement('foo');
        $element->setOptions([
            'format' => $format,
        ]);

        $this->assertSame($format, $element->getFormat());
    }

    public function testFailsWithInvalidMinSpecification()
    {
        $element = new DateTimeElement('foo');
        $element->setAttributes(
            [
            'inclusive' => true,
            'min'       => '2000-01-01T00',
            'step'      => '1',
            ]
        );

        $this->expectException(InvalidArgumentException::class);
        $element->getInputSpecification();
    }

    public function testFailsWithInvalidMaxSpecification()
    {
        $element = new DateTimeElement('foo');
        $element->setAttributes(
            [
            'inclusive' => true,
            'max'       => '2001-01-01T00',
            'step'      => '1',
            ]
        );
        $this->expectException(InvalidArgumentException::class);
        $element->getInputSpecification();
    }
}

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
use Zend\Form\Element\DateTimeSelect as DateTimeSelectElement;
use Zend\InputFilter\Factory as InputFilterFactory;

class DateTimeSelectTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new DateTimeSelectElement();

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);

        $expectedClasses = [
            'Zend\Validator\Date'
        ];
        foreach ($inputSpec['validators'] as $validator) {
            $class = get_class($validator);
            $this->assertContains($class, $expectedClasses, $class);
            switch ($class) {
                case 'Zend\Validator\Date':
                    $this->assertEquals('Y-m-d H:i:s', $validator->getFormat());
                    break;
                default:
                    break;
            }
        }
    }

    public function testInputSpecificationFilterIfSecondNotProvided()
    {
        $element = new DateTimeSelectElement('test');
        $factory = new InputFilterFactory();
        $inputFilter = $factory->createInputFilter([
            'test' => $element->getInputSpecification(),
        ]);
        $inputFilter->setData([
            'test' => [
                'year' => '2013',
                'month' => '02',
                'day' => '07',
                'hour' => '03',
                'minute' => '14'
            ],
        ]);
        $this->assertTrue($inputFilter->isValid());
    }

    public function testCanSetDateFromDateTime()
    {
        $element  = new DateTimeSelectElement();
        $element->setValue(new DateTime('2012-09-24 03:04:05'));

        $this->assertEquals('2012', $element->getYearElement()->getValue());
        $this->assertEquals('09', $element->getMonthElement()->getValue());
        $this->assertEquals('24', $element->getDayElement()->getValue());
        $this->assertEquals('03', $element->getHourElement()->getValue());
        $this->assertEquals('04', $element->getMinuteElement()->getValue());
        $this->assertEquals('05', $element->getSecondElement()->getValue());
    }

    public function testCanSetDateFromString()
    {
        $element  = new DateTimeSelectElement();
        $element->setValue('2012-09-24 03:04:05');

        $this->assertEquals('2012', $element->getYearElement()->getValue());
        $this->assertEquals('09', $element->getMonthElement()->getValue());
        $this->assertEquals('24', $element->getDayElement()->getValue());
        $this->assertEquals('03', $element->getHourElement()->getValue());
        $this->assertEquals('04', $element->getMinuteElement()->getValue());
        $this->assertEquals('05', $element->getSecondElement()->getValue());
    }

    public function testCanGetValue()
    {
        $element  = new DateTimeSelectElement();
        $element->setValue(new DateTime('2012-09-24 03:04:05'));

        $this->assertEquals('2012-09-24 03:04:05', $element->getValue());
    }

    /**
     * @expectedException \Zend\Form\Exception\InvalidArgumentException
     */
    public function testThrowsOnInvalidValue()
    {
        $element  = new DateTimeSelectElement();
        $element->setValue('hello world');
    }

    public function testUseDefaultValueForSecondsIfNotProvided()
    {
        $element  = new DateTimeSelectElement();
        $element->setValue([
            'year' => '2012',
            'month' => '09',
            'day' => '24',
            'hour' => '03',
            'minute' => '04'
        ]);

        $this->assertEquals('2012', $element->getYearElement()->getValue());
        $this->assertEquals('09', $element->getMonthElement()->getValue());
        $this->assertEquals('24', $element->getDayElement()->getValue());
        $this->assertEquals('03', $element->getHourElement()->getValue());
        $this->assertEquals('04', $element->getMinuteElement()->getValue());
        $this->assertEquals('00', $element->getSecondElement()->getValue());
    }

    public function testCloningPreservesCorrectValues()
    {
        $element = new DateTimeSelectElement();
        $element->setValue(new DateTime('2014-01-02 03:04:05'));

        $cloned = clone $element;

        $this->assertEquals('2014', $cloned->getYearElement()->getValue());
        $this->assertEquals('01', $cloned->getMonthElement()->getValue());
        $this->assertEquals('02', $cloned->getDayElement()->getValue());
        $this->assertEquals('03', $cloned->getHourElement()->getValue());
        $this->assertEquals('04', $cloned->getMinuteElement()->getValue());
        $this->assertEquals('05', $cloned->getSecondElement()->getValue());
    }

    public function testPassingNullValueToSetValueWillUseCurrentDate()
    {
        $now     = new DateTime;
        $element = new DateTimeSelectElement();
        $element->setValue(null);
        $yearElement = $element->getYearElement();
        $monthElement = $element->getMonthElement();
        $dayElement = $element->getDayElement();
        $this->assertEquals($now->format('Y'), $yearElement->getValue());
        $this->assertEquals($now->format('m'), $monthElement->getValue());
        $this->assertEquals($now->format('d'), $dayElement->getValue());
    }
}

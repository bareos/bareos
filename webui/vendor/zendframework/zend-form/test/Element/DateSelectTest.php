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
use Zend\Form\Element\DateSelect as DateSelectElement;

class DateSelectTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new DateSelectElement();

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
                    $this->assertEquals('Y-m-d', $validator->getFormat());
                    break;
                default:
                    break;
            }
        }
    }

    public function testCanSetDateFromDateTime()
    {
        $element  = new DateSelectElement();
        $element->setValue(new DateTime('2012-09-24'));

        $this->assertEquals('2012', $element->getYearElement()->getValue());
        $this->assertEquals('09', $element->getMonthElement()->getValue());
        $this->assertEquals('24', $element->getDayElement()->getValue());
    }

    public function testCanSetDateFromString()
    {
        $element  = new DateSelectElement();
        $element->setValue('2012-09-24');

        $this->assertEquals('2012', $element->getYearElement()->getValue());
        $this->assertEquals('09', $element->getMonthElement()->getValue());
        $this->assertEquals('24', $element->getDayElement()->getValue());
    }

    public function testCanGetValue()
    {
        $element  = new DateSelectElement();
        $element->setValue(new DateTime('2012-09-24'));

        $this->assertEquals('2012-09-24', $element->getValue());
    }

    /**
     * @expectedException \Zend\Form\Exception\InvalidArgumentException
     */
    public function testThrowsOnInvalidValue()
    {
        $element  = new DateSelectElement();
        $element->setValue('hello world');
    }

    public function testConstructAcceptsDayAttributes()
    {
        $sut = new DateSelectElement('dateSelect', ['day_attributes' => ['class' => 'test']]);
        $dayAttributes = $sut->getDayAttributes();
        $this->assertEquals('test', $dayAttributes['class']);
    }

    /**
     * @group 7114
     */
    public function testValueSetterReturnsSameObjectType()
    {
        $element = new DateSelectElement();

        $this->assertSame($element, $element->setValue('2014-01-01'));
    }
}

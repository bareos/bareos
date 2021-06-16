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
use Zend\Form\Element\Date as DateElement;
use Zend\Form\Exception\InvalidArgumentException;

/**
 * @covers \Zend\Form\Element\Date
 */
class DateTest extends TestCase
{
    /**
     * Stores the original set timezone
     *
     * @var string
     */
    private $originaltimezone;

    /**
     * {@inheritDoc}
     */
    public function setUp()
    {
        $this->originaltimezone = date_default_timezone_get();
    }

    /**
     * {@inheritDoc}
     */
    public function tearDown()
    {
        date_default_timezone_set($this->originaltimezone);
    }

    public function testProvidesDefaultInputSpecification()
    {
        $element = new DateElement('foo');

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);

        $expectedClasses = [
            'Zend\Validator\Date',
            'Zend\Validator\DateStep',
        ];
        foreach ($inputSpec['validators'] as $validator) {
            $class = get_class($validator);
            $this->assertContains($class, $expectedClasses, $class);
            switch ($class) {
                case 'Zend\Validator\DateStep':
                    $dateInterval = new \DateInterval('P1D');
                    $this->assertEquals($dateInterval, $validator->getStep());
                    $this->assertEquals(date('Y-m-d', 0), $validator->getBaseValue());
                    break;
                default:
                    break;
            }
        }
    }

    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new DateElement('foo');
        $element->setAttributes([
            'inclusive' => true,
            'min'       => '2000-01-01',
            'max'       => '2001-01-01',
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
                    $this->assertEquals('2000-01-01', $validator->getMin());
                    break;
                case 'Zend\Validator\LessThan':
                    $this->assertTrue($validator->getInclusive());
                    $this->assertEquals('2001-01-01', $validator->getMax());
                    break;
                case 'Zend\Validator\DateStep':
                    $dateInterval = new \DateInterval('P1D');
                    $this->assertEquals($dateInterval, $validator->getStep());
                    $this->assertEquals('2000-01-01', $validator->getBaseValue());
                    break;
                default:
                    break;
            }
        }
    }

    public function testValueReturnedFromComposedDateTimeIsRfc3339FullDateFormat()
    {
        $element = new DateElement('foo');
        $date    = new DateTime();
        $element->setValue($date);
        $value   = $element->getValue();
        $this->assertEquals($date->format('Y-m-d'), $value);
    }

    public function testCorrectFormatPassedToDateValidator()
    {
        $element = new DateElement('foo');
        $element->setAttributes([
            'min'       => '01-01-2012',
            'max'       => '31-12-2012',
        ]);
        $element->setFormat('d-m-Y');

        $inputSpec = $element->getInputSpecification();
        foreach ($inputSpec['validators'] as $validator) {
            switch (get_class($validator)) {
                case 'Zend\Validator\DateStep':
                case 'Zend\Validator\Date':
                    $this->assertEquals('d-m-Y', $validator->getFormat());
                    break;
            }
        }
    }

    /**
     * @group 6245
     */
    public function testStepValidatorIgnoresDaylightSavings()
    {
        date_default_timezone_set('Europe/London');

        $element   = new DateElement('foo');

        $inputSpec = $element->getInputSpecification();
        foreach ($inputSpec['validators'] as $validator) {
            switch (get_class($validator)) {
                case 'Zend\Validator\DateStep':
                    $this->assertTrue($validator->isValid('2013-12-25'));
                    break;
            }
        }
    }

    public function testFailsWithInvalidMinSpecification()
    {
        $element = new DateElement('foo');
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
        $element = new DateElement('foo');
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

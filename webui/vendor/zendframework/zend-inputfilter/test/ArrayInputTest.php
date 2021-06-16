<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\InputFilter;

use Zend\InputFilter\ArrayInput;
use Zend\InputFilter\Exception\InvalidArgumentException;

/**
 * @covers \Zend\InputFilter\ArrayInput
 */
class ArrayInputTest extends InputTest
{
    protected function setUp()
    {
        $this->input = new ArrayInput('foo');
    }

    public function testDefaultGetValue()
    {
        $this->assertCount(0, $this->input->getValue());
    }

    public function testArrayInputMarkedRequiredWithoutAFallbackFailsValidationForEmptyArrays()
    {
        $input = $this->input;
        $input->setRequired(true);
        $input->setValue([]);

        $this->assertFalse($input->isValid());
        $this->assertRequiredValidationErrorMessage($input);
    }

    public function testArrayInputMarkedRequiredWithoutAFallbackUsesProvidedErrorMessageOnFailureDueToEmptyArray()
    {
        $expected = 'error message';

        $input = $this->input;
        $input->setRequired(true);
        $input->setErrorMessage($expected);
        $input->setValue([]);

        $this->assertFalse($input->isValid());

        $messages = $input->getMessages();
        $this->assertCount(1, $messages);
        $message = array_pop($messages);
        $this->assertEquals($expected, $message);
    }

    public function testSetValueWithInvalidInputTypeThrowsInvalidArgumentException()
    {
        $this->expectException(InvalidArgumentException::class);
        $this->expectExceptionMessage('Value must be an array, string given');
        $this->input->setValue('bar');
    }

    public function fallbackValueVsIsValidProvider()
    {
        $dataSets = parent::fallbackValueVsIsValidProvider();
        array_walk($dataSets, function (&$set) {
            $set[1] = [$set[1]]; // Wrap fallback value into an array.
            $set[2] = [$set[2]]; // Wrap value into an array.
            $set[4] = [$set[4]]; // Wrap expected value into an array.
        });

        return $dataSets;
    }

    public function emptyValueProvider()
    {
        $dataSets = parent::emptyValueProvider();
        array_walk($dataSets, function (&$set) {
            $set['raw'] = [$set['raw']]; // Wrap value into an array.
        });

        return $dataSets;
    }

    public function mixedValueProvider()
    {
        $dataSets = parent::mixedValueProvider();
        array_walk($dataSets, function (&$set) {
            $set['raw'] = [$set['raw']]; // Wrap value into an array.
        });

        return $dataSets;
    }

    protected function createFilterChainMock(array $valueMap = [])
    {
        // ArrayInput filters per each array value
        $valueMap = array_map(
            function ($values) {
                if (is_array($values[0])) {
                    $values[0] = current($values[0]);
                }
                if (is_array($values[1])) {
                    $values[1] = current($values[1]);
                }
                return $values;
            },
            $valueMap
        );

        return parent::createFilterChainMock($valueMap);
    }

    protected function createValidatorChainMock(array $valueMap = [], $messages = [])
    {
        // ArrayInput validates per each array value
        $valueMap = array_map(
            function ($values) {
                if (is_array($values[0])) {
                    $values[0] = current($values[0]);
                }
                return $values;
            },
            $valueMap
        );

        return parent::createValidatorChainMock($valueMap, $messages);
    }

    protected function createNonEmptyValidatorMock($isValid, $value, $context = null)
    {
        // ArrayInput validates per each array value
        if (is_array($value)) {
            $value = current($value);
        }

        return parent::createNonEmptyValidatorMock($isValid, $value, $context);
    }

    protected function getDummyValue($raw = true)
    {
        return [parent::getDummyValue($raw)];
    }
}

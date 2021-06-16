<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */
namespace ZendTest\Form;

use PHPUnit\Framework\TestCase;

/**
 * @requires PHP 5.4
 * @group      Zend_Form
 */
class LabelAwareTraitTest extends TestCase
{
    public function testSetLabelAttributes()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $this->assertAttributeEquals(null, 'labelAttributes', $object);

        $labelAttributes = [
            'test',
            'test2',
        ];

        $object->setLabelAttributes($labelAttributes);

        $this->assertAttributeEquals($labelAttributes, 'labelAttributes', $object);
    }

    public function testGetEmptyLabelAttributes()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelAttributes = $object->getLabelAttributes();

        $this->assertEquals(null, $labelAttributes);
    }

    public function testGetLabelAttributes()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelAttributes = [
            'test',
            'test2',
        ];

        $object->setLabelAttributes($labelAttributes);

        $getLabelAttributes = $object->getLabelAttributes();

        $this->assertEquals($labelAttributes, $getLabelAttributes);
    }

    public function testSetEmptyLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = [];

        $object->setLabelOptions($labelOptions);

        $this->assertEquals($labelOptions, []);
    }

    public function testSetLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = [
            'foo' => 'bar',
            'foo2' => 'bar2',
        ];

        $object->setLabelOptions($labelOptions);

        $retrievedLabelOptions = $object->getLabelOptions();

        $this->assertEquals($retrievedLabelOptions, $labelOptions);
    }

    public function testGetEmptyLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = $object->getLabelOptions();

        $this->assertEquals($labelOptions, []);
    }

    public function testGetLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = [
            'foo' => 'bar',
            'foo2' => 'bar',
        ];

        $object->setLabelOptions($labelOptions);

        $retrievedLabelOptions = $object->getLabelOptions();

        $this->assertEquals($labelOptions, $retrievedLabelOptions);
    }

    public function testClearLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = [
            'foo' => 'bar',
            'foo2' => 'bar',
        ];

        $object->setLabelOptions($labelOptions);

        $object->clearLabelOptions();

        $retrievedLabelOptions = $object->getLabelOptions();

        $this->assertEquals([], $retrievedLabelOptions);
    }

    public function testRemoveLabelOptions()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $labelOptions = [
            'foo' => 'bar',
            'foo2' => 'bar2',
        ];

        $object->setLabelOptions($labelOptions);

        $toRemoveLabelOptions = [
            'foo',
        ];

        $object->removeLabelOptions($toRemoveLabelOptions);

        $expectedLabelOptions = [
            'foo2' => 'bar2',
        ];

        $retrievedLabelOptions = $object->getLabelOptions();

        $this->assertEquals($expectedLabelOptions, $retrievedLabelOptions);
    }

    public function testSetLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $object->setLabelOption('foo', 'bar');

        $expectedLabelOptions = [
            'foo' => 'bar',
        ];

        $retrievedLabelOptions = $object->getLabelOptions();

        $this->assertEquals($expectedLabelOptions, $retrievedLabelOptions);
    }

    public function testGetInvalidLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $invalidOption = 'foo';

        $retrievedOption = $object->getLabelOption($invalidOption);

        $this->assertEquals(null, $retrievedOption);
    }

    public function testGetLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $option = 'foo';
        $value = 'bar';

        $object->setLabelOption($option, $value);

        $retrievedValue = $object->getLabelOption($option);

        $this->assertEquals($retrievedValue, $value);
    }

    public function testRemoveLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $option = 'foo';
        $value = 'bar';

        $object->setLabelOption($option, $value);

        $object->removeLabelOption($option);

        $retrievedValue = $object->getLabelOption($option);

        $this->assertEquals(null, $retrievedValue);
    }

    public function testHasValidLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $option = 'foo';
        $value = 'bar';

        $object->setLabelOption($option, $value);

        $hasLabelOptionResult = $object->hasLabelOption($option);
        $this->assertTrue($hasLabelOptionResult);
    }

    public function testHasInvalidLabelOption()
    {
        $object = $this->getObjectForTrait('\Zend\Form\LabelAwareTrait');

        $option = 'foo';

        $hasLabelOptionResult = $object->hasLabelOption($option);
        $this->assertFalse($hasLabelOptionResult);
    }
}

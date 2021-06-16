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
use Zend\Form\Element;

class ElementTest extends TestCase
{
    public function testAttributesAreEmptyByDefault()
    {
        $element = new Element();
        $this->assertEquals([], $element->getAttributes());
    }

    public function testLabelAttributesAreEmptyByDefault()
    {
        $element = new Element();
        $this->assertEquals([], $element->getLabelAttributes());
    }

    public function testCanAddAttributesSingly()
    {
        $element = new Element();
        $element->setAttribute('data-foo', 'bar');
        $this->assertEquals('bar', $element->getAttribute('data-foo'));
    }

    public function testCanAddManyAttributesAtOnce()
    {
        $element = new Element();
        $attributes = [
            'type'     => 'text',
            'class'    => 'text-element',
            'data-foo' => 'bar',
            'x-autocompletetype' => 'email'
        ];
        $element->setAttributes($attributes);
        $this->assertEquals($attributes, $element->getAttributes());
    }

    public function testAddingAttributesMerges()
    {
        $element = new Element();
        $attributes = [
            'type'     => 'text',
            'class'    => 'text-element',
            'data-foo' => 'bar',
        ];
        $attributesExtra = [
            'data-foo' => 'baz',
            'width'    => 20,
        ];
        $element->setAttributes($attributes);
        $element->setAttributes($attributesExtra);
        $expected = array_merge($attributes, $attributesExtra);
        $this->assertEquals($expected, $element->getAttributes());
    }

    public function testCanClearAllAttributes()
    {
        $element = new Element();
        $attributes = [
            'type'     => 'text',
            'class'    => 'text-element',
            'data-foo' => 'bar',
        ];
        $element->setAttributes($attributes);
        $element->clearAttributes();
        $this->assertEquals([], $element->getAttributes());
    }

    public function testCanRemoveSingleAttribute()
    {
        $element = new Element();
        $attributes = [
            'type'     => 'text',
            'class'    => 'text-element',
            'data-foo' => 'bar',
        ];
        $element->setAttributes($attributes);
        $element->removeAttribute('type');
        $this->assertFalse($element->hasAttribute('type'));
    }

    public function testCanRemoveMultipleAttributes()
    {
        $element = new Element();
        $attributes = [
            'type'     => 'text',
            'class'    => 'text-element',
            'data-foo' => 'bar',
        ];
        $element->setAttributes($attributes);
        $element->removeAttributes(['type', 'class']);
        $this->assertFalse($element->hasAttribute('type'));
        $this->assertFalse($element->hasAttribute('class'));
    }

    public function testSettingNameSetsNameAttribute()
    {
        $element = new Element();
        $element->setName('foo');
        $this->assertEquals('foo', $element->getAttribute('name'));
    }

    public function testSettingNameAttributeAllowsRetrievingName()
    {
        $element = new Element();
        $element->setAttribute('name', 'foo');
        $this->assertEquals('foo', $element->getName());
    }

    public function testCanPassNameToConstructor()
    {
        $element = new Element('foo');
        $this->assertEquals('foo', $element->getName());
    }

    public function testCanSetCustomOptionFromConstructor()
    {
        $element = new Element('foo', [
            'custom' => 'option'
        ]);
        $options = $element->getOptions();
        $this->assertArrayHasKey('custom', $options);
        $this->assertEquals('option', $options['custom']);
    }

    public function testCanSetCustomOptionFromMethod()
    {
        $element = new Element('foo');
        $element->setOptions([
            'custom' => 'option'
        ]);

        $options = $element->getOptions();
        $this->assertArrayHasKey('custom', $options);
        $this->assertEquals('option', $options['custom']);
    }

    public function testCanRetrieveSpecificOption()
    {
        $element = new Element('foo');
        $element->setOptions([
            'custom' => 'option'
        ]);
        $option = $element->getOption('custom');
        $this->assertEquals('option', $option);
    }

    public function testSpecificOptionsSetLabelAttributes()
    {
        $element = new Element('foo');
        $element->setOptions([
            'label' => 'foo',
            'label_attributes' => ['bar' => 'baz']
        ]);
        $option = $element->getOption('label_attributes');
        $this->assertEquals(['bar' => 'baz'], $option);
    }

    public function testLabelOptionsAccessors()
    {
        $element = new Element('foo');
        $element->setOptions([
            'label_options' => ['moar' => 'foo']
        ]);

        $labelOptions = $element->getLabelOptions();
        $this->assertEquals(['moar' => 'foo'], $labelOptions);
    }

    public function testCanSetSingleOptionForLabel()
    {
        $element = new Element('foo');
        $element->setOption('label', 'foo');
        $option = $element->getOption('label');
        $this->assertEquals('foo', $option);
    }

    public function testSetOptionsWrongInputRaisesException()
    {
        $element = new Element('foo');

        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $element->setOptions(null);
    }

    public function testSetOptionsIsTraversable()
    {
        $element = new Element('foo');
        $element->setOptions(new \ArrayObject(['foo' => 'bar']));
        $this->assertEquals('foo', $element->getName());
        $this->assertEquals(['foo' => 'bar'], $element->getOptions());
    }

    public function testGetOption()
    {
        $element = new Element('foo');
        $this->assertNull($element->getOption('foo'));
    }

    public function testSetAttributesWrongInputRaisesException()
    {
        $element = new Element('foo');

        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $element->setAttributes(null);
    }

    public function testSetMessagesWrongInputRaisesException()
    {
        $element = new Element('foo');

        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $element->setMessages(null);
    }

    public function testLabelOptionsAreEmptyByDefault()
    {
        $element = new Element();
        $this->assertEquals([], $element->getLabelOptions());
    }

    public function testLabelOptionsCanBeSetViaOptionsArray()
    {
        $element = new Element('foo');
        $element->setOptions([
            'label_options' => ['moar' => 'foo']
        ]);

        $this->assertEquals('foo', $element->getLabelOption('moar'));
    }

    public function testCanAddLabelOptionSingly()
    {
        $element = new Element();
        $element->setLabelOption('foo', 'bar');
        $this->assertEquals('bar', $element->getLabelOption('foo'));
    }

    public function testCanAddManyLabelOptionsAtOnce()
    {
        $element = new Element();
        $options = [
            'foo'     => 'bar',
            'foo2'    => 'baz'
        ];
        $element->setLabelOptions($options);

        // check each expected key individually
        foreach ($options as $k => $v) {
            $this->assertEquals($v, $element->getLabelOption($k));
        }
    }

    /**
     * @expectedException InvalidArgumentException
     */
    public function testPassingWrongArgumentToSetLabelOptionsThrowsException()
    {
        $element = new Element();
        $element->setLabelOptions(null);
    }

    public function testSettingLabelOptionsMerges()
    {
        $element = new Element();
        $options = [
            'foo'     => 'bar',
            'foo2'    => 'baz'
        ];
        $optionsExtra = [
            'foo3'    => 'bar2',
            'foo2'    => 'baz2'
        ];
        $element->setLabelOptions($options);
        $element->setLabelOptions($optionsExtra);
        $expected = array_merge($options, $optionsExtra);

        // check each expected key individually
        foreach ($expected as $k => $v) {
            $this->assertEquals($v, $element->getLabelOption($k));
        }
    }

    public function testCanClearAllLabelOptions()
    {
        $element = new Element();
        $options = [
            'foo'     => 'bar',
            'foo2'    => 'baz'
        ];
        $element->setLabelOptions($options);
        $element->clearLabelOptions();
        $this->assertEquals([], $element->getLabelOptions());
    }

    public function testCanRemoveSingleLabelOption()
    {
        $element = new Element();
        $options = [
            'foo'     => 'bar',
            'foo2'    => 'baz'
        ];
        $element->setLabelOptions($options);
        $element->removeLabelOption('foo2');
        $this->assertFalse($element->hasLabelOption('foo2'));
    }

    public function testCanRemoveMultipleLabelOptions()
    {
        $element = new Element();
        $options = [
            'foo'     => 'bar',
            'foo2'    => 'baz',
            'foo3'    => 'bar2'
        ];
        $element->setLabelOptions($options);
        $element->removeLabelOptions(['foo', 'foo2']);
        $this->assertFalse($element->hasLabelOption('foo'));
        $this->assertFalse($element->hasLabelOption('foo2'));
        $this->assertTrue($element->hasLabelOption('foo3'));
    }

    public function testCanAddMultipleAriaAttributes()
    {
        $element = new Element();
        $attributes = [
            'type'             => 'text',
            'aria-label'       => 'alb',
            'aria-describedby' => 'adb',
            'aria-orientation' => 'vertical'
        ];
        $element->setAttributes($attributes);
        $this->assertTrue($element->hasAttribute('aria-describedby'));
        $this->assertTrue($element->hasAttribute('aria-label'));
        $this->assertTrue($element->hasAttribute('aria-orientation'));
    }

    public function testCanRemoveMultipleAriaAttributes()
    {
        $element = new Element();
        $attributes = [
            'type'             => 'text',
            'aria-label'       => 'alb',
            'aria-describedby' => 'adb',
            'aria-orientation' => 'vertical'
        ];
        $element->setAttributes($attributes);
        $element->removeAttributes(['aria-label', 'aria-describedby', 'aria-orientation']);
        $this->assertFalse($element->hasAttribute('aria-describedby'));
        $this->assertFalse($element->hasAttribute('aria-label'));
        $this->assertFalse($element->hasAttribute('aria-orientation'));
    }
}

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
use Zend\Form\Form;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilter;
use Zend\Hydrator;

class FieldsetTest extends TestCase
{
    public function setUp()
    {
        $this->fieldset = new Fieldset();
    }

    public function populateFieldset()
    {
        $this->fieldset->add(new Element('foo'));
        $this->fieldset->add(new Element('bar'));
        $this->fieldset->add(new Element('baz'));

        $subFieldset = new Fieldset('foobar');
        $subFieldset->add(new Element('foo'));
        $subFieldset->add(new Element('bar'));
        $subFieldset->add(new Element('baz'));
        $this->fieldset->add($subFieldset);

        $subFieldset = new Fieldset('barbaz');
        $subFieldset->add(new Element('foo'));
        $subFieldset->add(new Element('bar'));
        $subFieldset->add(new Element('baz'));
        $this->fieldset->add($subFieldset);
    }

    public function getMessages()
    {
        return [
            'foo' => [
                'Foo message 1',
            ],
            'bar' => [
                'Bar message 1',
                'Bar message 2',
            ],
            'baz' => [
                'Baz message 1',
            ],
            'foobar' => [
                'foo' => [
                    'Foo message 1',
                ],
                'bar' => [
                    'Bar message 1',
                    'Bar message 2',
                ],
                'baz' => [
                    'Baz message 1',
                ],
            ],
            'barbaz' => [
                'foo' => [
                    'Foo message 1',
                ],
                'bar' => [
                    'Bar message 1',
                    'Bar message 2',
                ],
                'baz' => [
                    'Baz message 1',
                ],
            ],
        ];
    }

    public function testExtractOnAnEmptyRelationship()
    {
        $form = new TestAsset\FormCollection();
        $form->populateValues(['fieldsets' => []]);

        $this->addToAssertionCount(1);
    }

    public function testExtractOnAnEmptyTraversable()
    {
        $form = new TestAsset\FormCollection();
        $form->populateValues(new \ArrayObject(['fieldsets' => new \ArrayObject()]));

        $this->addToAssertionCount(1);
    }

    public function testTraversableAcceptedValueForFieldset()
    {
        $subValue = new \ArrayObject(['field' => 'value']);
        $subFieldset = new TestAsset\ValueStoringFieldset('subFieldset');
        $this->fieldset->add($subFieldset);
        $this->fieldset->populateValues(['subFieldset' => $subValue]);
        $this->assertEquals($subValue, $subFieldset->getStoredValue());
    }

    public function testPopulateValuesWithInvalidElementRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->populateValues(null);
    }

    public function testFieldsetIsEmptyByDefault()
    {
        $this->assertEquals(0, count($this->fieldset));
    }

    public function testCanAddElementsToFieldset()
    {
        $this->fieldset->add(new Element('foo'));
        $this->assertEquals(1, count($this->fieldset));
    }

    public function testCanSetCustomOptionFromConstructor()
    {
        $fieldset = new Fieldset('foo', [
            'custom' => 'option'
        ]);
        $options = $fieldset->getOptions();
        $this->assertArrayHasKey('custom', $options);
        $this->assertEquals('option', $options['custom']);
    }

    public function testAddWithInvalidElementRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->add(null);
    }

    public function testCanGrabElementByNameWhenNotProvidedWithAlias()
    {
        $element = new Element('foo');
        $this->fieldset->add($element);
        $this->assertSame($element, $this->fieldset->get('foo'));
    }

    public function testElementMayBeRetrievedByAliasProvidedWhenAdded()
    {
        $element = new Element('foo');
        $this->fieldset->add($element, ['name' => 'bar']);
        $this->assertSame($element, $this->fieldset->get('bar'));
    }

    public function testElementNameIsChangedToAliasWhenAdded()
    {
        $element = new Element('foo');
        $this->fieldset->add($element, ['name' => 'bar']);
        $this->assertEquals('bar', $element->getName());
    }

    public function testCannotRetrieveElementByItsNameWhenProvidingAnAliasDuringAddition()
    {
        $element = new Element('foo');
        $this->fieldset->add($element, ['name' => 'bar']);
        $this->assertFalse($this->fieldset->has('foo'));
    }

    public function testAddingAnElementWithNoNameOrAliasWillRaiseException()
    {
        $element = new Element();
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->add($element);
    }

    public function testCanAddFieldsetsToFieldset()
    {
        $fieldset = new Fieldset('foo');
        $this->fieldset->add($fieldset);
        $this->assertEquals(1, count($this->fieldset));
    }

    public function testCanRemoveElementsByName()
    {
        $element = new Element('foo');
        $this->fieldset->add($element);
        $this->assertTrue($this->fieldset->has('foo'));
        $this->fieldset->remove('foo');
        $this->assertFalse($this->fieldset->has('foo'));
    }

    public function testCanRemoveFieldsetsByName()
    {
        $fieldset = new Fieldset('foo');
        $this->fieldset->add($fieldset);
        $this->assertTrue($this->fieldset->has('foo'));
        $this->fieldset->remove('foo');
        $this->assertFalse($this->fieldset->has('foo'));
    }

    public function testCanRemoveElementsByWrongName()
    {
        $element = new Element('foo');
        $this->fieldset->add($element);
        $element2 = new Element('bar');
        $this->fieldset->add($element2);
        $this->assertTrue($this->fieldset->has('foo'));
        $this->assertTrue($this->fieldset->has('bar'));

        // remove wrong element, bar still available
        $this->fieldset->remove('bars');
        $this->assertTrue($this->fieldset->has('foo'));
        $this->assertTrue($this->fieldset->has('bar'));

        $this->fieldset->remove('bar');
        $this->assertTrue($this->fieldset->has('foo'));
        $this->assertFalse($this->fieldset->has('bar'));
    }

    public function testCanRetrieveAllAttachedElementsSeparateFromFieldsetsAtOnce()
    {
        $this->populateFieldset();
        $elements = $this->fieldset->getElements();
        $this->assertEquals(3, count($elements));
        foreach (['foo', 'bar', 'baz'] as $name) {
            $this->assertTrue(isset($elements[$name]));
            $element = $this->fieldset->get($name);
            $this->assertSame($element, $elements[$name]);
        }
    }

    public function testCanRetrieveAllAttachedFieldsetsSeparateFromElementsAtOnce()
    {
        $this->populateFieldset();
        $fieldsets = $this->fieldset->getFieldsets();
        $this->assertEquals(2, count($fieldsets));
        foreach (['foobar', 'barbaz'] as $name) {
            $this->assertTrue(isset($fieldsets[$name]));
            $fieldset = $this->fieldset->get($name);
            $this->assertSame($fieldset, $fieldsets[$name]);
        }
    }

    public function testCanSetAndRetrieveErrorMessagesForAllElementsAndFieldsets()
    {
        $this->populateFieldset();
        $messages = $this->getMessages();
        $this->fieldset->setMessages($messages);
        $test = $this->fieldset->getMessages();
        $this->assertEquals($messages, $test);
    }

    public function testSetMessagesWithInvalidElementRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->setMessages(null);
    }

    public function testOnlyElementsWithErrorsInMessages()
    {
        $fieldset = new TestAsset\FieldsetWithInputFilter('set');
        $fieldset->add(new Element('foo'));
        $fieldset->add(new Element('bar'));

        $form = new Form();
        $form->add($fieldset);
        $form->setInputFilter(new InputFilter());
        $form->setData([]);
        $form->isValid();

        $messages = $form->getMessages();
        $this->assertArrayHasKey('foo', $messages['set']);
        $this->assertArrayNotHasKey('bar', $messages['set']);
    }

    public function testCanRetrieveMessagesForSingleElementsAfterMessagesHaveBeenSet()
    {
        $this->populateFieldset();
        $messages = $this->getMessages();
        $this->fieldset->setMessages($messages);

        $test = $this->fieldset->getMessages('bar');
        $this->assertEquals($messages['bar'], $test);
    }

    public function testCanRetrieveMessagesForSingleFieldsetsAfterMessagesHaveBeenSet()
    {
        $this->populateFieldset();
        $messages = $this->getMessages();
        $this->fieldset->setMessages($messages);

        $test = $this->fieldset->getMessages('barbaz');
        $this->assertEquals($messages['barbaz'], $test);
    }

    public function testGetMessagesWithInvalidElementRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->getMessages('foo');
    }

    public function testCountGivesCountOfAttachedElementsAndFieldsets()
    {
        $this->populateFieldset();
        $this->assertEquals(5, count($this->fieldset));
    }

    public function testCanIterateOverElementsAndFieldsetsInOrderAttached()
    {
        $this->populateFieldset();
        $expected = ['foo', 'bar', 'baz', 'foobar', 'barbaz'];
        $test     = [];
        foreach ($this->fieldset as $element) {
            $test[] = $element->getName();
        }
        $this->assertEquals($expected, $test);
    }

    public function testIteratingRespectsOrderPriorityProvidedWhenAttaching()
    {
        $this->fieldset->add(new Element('foo'), ['priority' => 10]);
        $this->fieldset->add(new Element('bar'), ['priority' => 20]);
        $this->fieldset->add(new Element('baz'), ['priority' => -10]);
        $this->fieldset->add(new Fieldset('barbaz'), ['priority' => 30]);

        $expected = ['barbaz', 'bar', 'foo', 'baz'];
        $test     = [];
        foreach ($this->fieldset as $element) {
            $test[] = $element->getName();
        }
        $this->assertEquals($expected, $test);
    }

    public function testIteratingRespectsOrderPriorityProvidedWhenSetLater()
    {
        $this->fieldset->add(new Element('foo'), ['priority' => 10]);
        $this->fieldset->add(new Element('bar'), ['priority' => 20]);
        $this->fieldset->add(new Element('baz'), ['priority' => -10]);
        $this->fieldset->add(new Fieldset('barbaz'), ['priority' => 30]);
        $this->fieldset->setPriority('baz', 99);

        $expected = ['baz', 'barbaz', 'bar', 'foo'];
        $test     = [];
        foreach ($this->fieldset as $element) {
            $test[] = $element->getName();
        }
        $this->assertEquals($expected, $test);
    }

    public function testIteratingRespectsOrderPriorityWhenCloned()
    {
        $this->fieldset->add(new Element('foo'), ['priority' => 10]);
        $this->fieldset->add(new Element('bar'), ['priority' => 20]);
        $this->fieldset->add(new Element('baz'), ['priority' => -10]);
        $this->fieldset->add(new Fieldset('barbaz'), ['priority' => 30]);

        $expected = ['barbaz', 'bar', 'foo', 'baz'];

        $testOrig  = [];
        $testClone = [];

        $fieldsetClone = clone $this->fieldset;

        foreach ($this->fieldset as $element) {
            $testOrig[] = $element->getName();
        }

        foreach ($fieldsetClone as $element) {
            $testClone[] = $element->getName();
        }

        $this->assertEquals($expected, $testClone);
        $this->assertEquals($testOrig, $testClone);
    }

    public function testCloneDeepClonesElementsAndObject()
    {
        $this->fieldset->add(new Element('foo'));
        $this->fieldset->add(new Element('bar'));
        $this->fieldset->setObject(new \stdClass);

        $fieldsetClone = clone $this->fieldset;

        $this->assertNotSame($this->fieldset->get('foo'), $fieldsetClone->get('foo'));
        $this->assertNotSame($this->fieldset->get('bar'), $fieldsetClone->get('bar'));
        $this->assertNotSame($this->fieldset->getObject(), $fieldsetClone->getObject());
    }

    public function testSubFieldsetsBindObject()
    {
        $form = new Form();
        $fieldset = new Fieldset('foobar');
        $form->add($fieldset);
        $value = new \ArrayObject([
            'foobar' => 'abc',
        ]);
        $value['foobar'] = new \ArrayObject([
            'foo' => 'abc'
        ]);
        $form->bind($value);
        $this->assertSame($fieldset, $form->get('foobar'));
    }

    public function testBindEmptyValue()
    {
        $value = new \ArrayObject([
            'foo' => 'abc',
            'bar' => 'def',
        ]);

        $inputFilter = new InputFilter();
        $inputFilter->add(['name' => 'foo', 'required' => false]);
        $inputFilter->add(['name' => 'bar', 'required' => false]);

        $form = new Form();
        $form->add(new Element('foo'));
        $form->add(new Element('bar'));
        $form->setInputFilter($inputFilter);
        $form->bind($value);
        $form->setData([
            'foo' => '',
            'bar' => 'ghi',
        ]);
        $form->isValid();

        $this->assertSame('', $value['foo']);
        $this->assertSame('ghi', $value['bar']);
    }

    public function testFieldsetExposesFluentInterface()
    {
        $fieldset = $this->fieldset->add(new Element('foo'));
        $this->assertSame($this->fieldset, $fieldset);
        $fieldset = $this->fieldset->remove('foo');
        $this->assertSame($this->fieldset, $fieldset);
    }

    public function testSetOptions()
    {
        $this->fieldset->setOptions([
                                   'foo' => 'bar'
                              ]);
        $option = $this->fieldset->getOption('foo');

        $this->assertEquals('bar', $option);
    }

    public function testSetOptionsUseAsBaseFieldset()
    {
        $this->fieldset->setOptions([
                                   'use_as_base_fieldset' => 'bar'
                              ]);
        $option = $this->fieldset->getOption('use_as_base_fieldset');

        $this->assertEquals('bar', $option);
    }

    public function testSetOptionAllowedObjectBindingClass()
    {
        $this->fieldset->setOptions([
                                         'allowed_object_binding_class' => 'bar'
                                    ]);
        $option = $this->fieldset->getOption('allowed_object_binding_class');

        $this->assertEquals('bar', $option);
    }

    /**
     * @expectedException Zend\Form\Exception\InvalidElementException
     */
    public function testShouldThrowExceptionWhenGetInvalidElement()
    {
        $this->fieldset->get('doesnt_exist');
    }

    public function testBindValuesHasNoName()
    {
        $bindValues = $this->fieldset->bindValues(['foo']);
        $this->assertNull($bindValues);
    }

    public function testBindValuesSkipDisabled()
    {
        $object = new \stdClass();
        $object->disabled = 'notModified';
        $object->not_disabled = 'notModified';

        $textInput = new Element\Text('not_disabled');
        $disabledInput = new Element\Text('disabled');
        $disabledInput->setAttribute('disabled', 'disabled');

        $form = new Form();
        $form->add($textInput);
        $form->add($disabledInput);

        $form->setObject($object);
        $form->setHydrator(
            class_exists(Hydrator\ObjectPropertyHydrator::class)
            ? new Hydrator\ObjectPropertyHydrator()
            : new Hydrator\ObjectProperty()
        );
        $form->bindValues(['not_disabled' => 'modified', 'disabled' => 'modified']);

        $this->assertEquals('modified', $object->not_disabled);
        $this->assertEquals('notModified', $object->disabled);
    }

    /**
     * @group 7109
     */
    public function testBindValuesDoesNotSkipElementsWithFalsyDisabledValues()
    {
        $object = new \stdClass();
        $object->disabled = 'notModified';
        $object->not_disabled = 'notModified';

        $textInput = new Element\Text('not_disabled');
        $disabledInput = new Element\Text('disabled');
        $disabledInput->setAttribute('disabled', '');

        $form = new Form();
        $form->add($textInput);
        $form->add($disabledInput);

        $form->setObject($object);
        $form->setHydrator(
            class_exists(Hydrator\ObjectPropertyHydrator::class)
            ? new Hydrator\ObjectPropertyHydrator()
            : new Hydrator\ObjectProperty()
        );
        $form->bindValues(['not_disabled' => 'modified', 'disabled' => 'modified']);

        $this->assertEquals('modified', $object->not_disabled);
        $this->assertEquals('modified', $object->disabled);
    }

    public function testSetObjectWithStringRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->fieldset->setObject('foo');
    }

    public function testShouldValidateAllowObjectBindingByClassname()
    {
        $object = new \stdClass();
        $this->fieldset->setAllowedObjectBindingClass('stdClass');
        $allowed = $this->fieldset->allowObjectBinding($object);

        $this->assertTrue($allowed);
    }

    public function testShouldValidateAllowObjectBindingByObject()
    {
        $object = new \stdClass();
        $this->fieldset->setObject($object);
        $allowed = $this->fieldset->allowObjectBinding($object);

        $this->assertTrue($allowed);
    }

    /**
     * @group 6585
     * @group 6614
     */
    public function testBindValuesPreservesNewValueAfterValidation()
    {
        $form = new Form();
        $form->add(new Element('foo'));
        $form->setHydrator(
            class_exists(Hydrator\ObjectPropertyHydrator::class)
            ? new Hydrator\ObjectPropertyHydrator()
            : new Hydrator\ObjectProperty()
        );

        $object      = new \stdClass();
        $object->foo = 'Initial value';
        $form->bind($object);

        $form->setData([
            'foo' => 'New value'
        ]);

        $this->assertSame('New value', $form->get('foo')->getValue());

        $form->isValid();

        $this->assertSame('New value', $form->get('foo')->getValue());
    }

    /**
     * Error test for https://github.com/zendframework/zend-form/issues/135
     */
    public function testSetAndGetErrorMessagesForNonExistentElements()
    {
        $messages = [
            'foo' => [
                'foo_message_key' => 'foo_message_val',
            ],
            'bar' => [
                'bar_message_key' => 'bar_message_val',
            ],
        ];

        $fieldset = new Fieldset();
        $fieldset->setMessages($messages);

        $this->assertEquals($messages, $fieldset->getMessages());
    }
}

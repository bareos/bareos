<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\View\Helper;

use Zend\Form\Element;
use Zend\Form\View\Helper\FormRange as FormRangeHelper;

class FormRangeTest extends CommonTestCase
{
    public function setUp()
    {
        $this->helper = new FormRangeHelper();
        parent::setUp();
    }

    public function testRaisesExceptionWhenNameIsNotPresentInElement()
    {
        $element = new Element();
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('name');
        $this->helper->render($element);
    }

    public function testGeneratesNumberInputTagWithElement()
    {
        $element = new Element('foo');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="range"', $markup);
    }

    public function testGeneratesNumberInputTagRegardlessOfElementType()
    {
        $element = new Element('foo');
        $element->setAttribute('type', 'radio');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="range"', $markup);
    }

    public function validAttributes()
    {
        return [
            ['name', 'assertContains'],
            ['accept', 'assertNotContains'],
            ['alt', 'assertNotContains'],
            ['autocomplete', 'assertContains'],
            ['autofocus', 'assertContains'],
            ['checked', 'assertNotContains'],
            ['dirname', 'assertNotContains'],
            ['disabled', 'assertContains'],
            ['form', 'assertContains'],
            ['formaction', 'assertNotContains'],
            ['formenctype', 'assertNotContains'],
            ['formmethod', 'assertNotContains'],
            ['formnovalidate', 'assertNotContains'],
            ['formtarget', 'assertNotContains'],
            ['height', 'assertNotContains'],
            ['list', 'assertContains'],
            ['max', 'assertContains'],
            ['maxlength', 'assertNotContains'],
            ['min', 'assertContains'],
            ['multiple', 'assertNotContains'],
            ['pattern', 'assertNotContains'],
            ['placeholder', 'assertNotContains'],
            ['readonly', 'assertNotContains'],
            ['required', 'assertContains'],
            ['size', 'assertNotContains'],
            ['src', 'assertNotContains'],
            ['step', 'assertContains'],
            ['value', 'assertContains'],
            ['width', 'assertNotContains'],
        ];
    }

    public function getCompleteElement()
    {
        $element = new Element('foo');
        $element->setAttributes([
            'accept'             => 'value',
            'alt'                => 'value',
            'autocomplete'       => 'on',
            'autofocus'          => 'autofocus',
            'checked'            => 'checked',
            'dirname'            => 'value',
            'disabled'           => 'disabled',
            'form'               => 'value',
            'formaction'         => 'value',
            'formenctype'        => 'value',
            'formmethod'         => 'value',
            'formnovalidate'     => 'value',
            'formtarget'         => 'value',
            'height'             => 'value',
            'id'                 => 'value',
            'list'               => 'value',
            'max'                => '10',
            'maxlength'          => 'value',
            'min'                => '0',
            'multiple'           => 'multiple',
            'name'               => 'value',
            'pattern'            => 'value',
            'placeholder'        => 'value',
            'readonly'           => 'readonly',
            'required'           => 'required',
            'size'               => 'value',
            'src'                => 'value',
            'step'               => '1',
            'width'              => 'value',
        ]);
        $element->setValue('value');
        return $element;
    }

    /**
     * @dataProvider validAttributes
     */
    public function testAllValidFormMarkupAttributesPresentInElementAreRendered($attribute, $assertion)
    {
        $element = $this->getCompleteElement();
        $markup  = $this->helper->render($element);
        switch ($attribute) {
            case 'value':
                $expect  = sprintf('%s="%s"', $attribute, $element->getValue());
                break;
            default:
                $expect  = sprintf('%s="%s"', $attribute, $element->getAttribute($attribute));
                break;
        }
        $this->$assertion($expect, $markup);
    }

    public function testInvokeProxiesToRender()
    {
        $element = new Element('foo');
        $markup  = $this->helper->__invoke($element);
        $this->assertContains('<input', $markup);
        $this->assertContains('name="foo"', $markup);
        $this->assertContains('type="range"', $markup);
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }
}

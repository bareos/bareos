<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\View\Helper;

use Zend\Form\Element;
use Zend\Form\View\Helper\FormSearch as FormSearchHelper;

class FormSearchTest extends CommonTestCase
{
    public function setUp()
    {
        $this->helper = new FormSearchHelper();
        parent::setUp();
    }

    public function testRaisesExceptionWhenNameIsNotPresentInElement()
    {
        $element = new Element();
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('name');
        $this->helper->render($element);
    }

    public function testGeneratesTextInputTagWithElement()
    {
        $element = new Element('foo');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="search"', $markup);
    }

    public function testGeneratesTextInputTagRegardlessOfElementType()
    {
        $element = new Element('foo');
        $element->setAttribute('type', 'email');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="search"', $markup);
    }

    public function validAttributes()
    {
        return [
            'name'           => ['name', 'assertContains'],
            'accept'         => ['accept', 'assertNotContains'],
            'alt'            => ['alt', 'assertNotContains'],
            'autocomplete'   => ['autocomplete', 'assertContains'],
            'autofocus'      => ['autofocus', 'assertContains'],
            'checked'        => ['checked', 'assertNotContains'],
            'dirname'        => ['dirname', 'assertContains'],
            'disabled'       => ['disabled', 'assertContains'],
            'form'           => ['form', 'assertContains'],
            'formaction'     => ['formaction', 'assertNotContains'],
            'formenctype'    => ['formenctype', 'assertNotContains'],
            'formmethod'     => ['formmethod', 'assertNotContains'],
            'formnovalidate' => ['formnovalidate', 'assertNotContains'],
            'formtarget'     => ['formtarget', 'assertNotContains'],
            'height'         => ['height', 'assertNotContains'],
            'list'           => ['list', 'assertContains'],
            'max'            => ['max', 'assertNotContains'],
            'maxlength'      => ['maxlength', 'assertContains'],
            'min'            => ['min', 'assertNotContains'],
            'minlength'      => ['minlength', 'assertContains'],
            'multiple'       => ['multiple', 'assertNotContains'],
            'pattern'        => ['pattern', 'assertContains'],
            'placeholder'    => ['placeholder', 'assertContains'],
            'readonly'       => ['readonly', 'assertContains'],
            'required'       => ['required', 'assertContains'],
            'size'           => ['size', 'assertContains'],
            'src'            => ['src', 'assertNotContains'],
            'step'           => ['step', 'assertNotContains'],
            'value'          => ['value', 'assertContains'],
            'width'          => ['width', 'assertNotContains'],
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
            'max'                => 'value',
            'maxlength'          => 'value',
            'min'                => 'value',
            'minlength'          => 'value',
            'multiple'           => 'multiple',
            'name'               => 'value',
            'pattern'            => 'value',
            'placeholder'        => 'value',
            'readonly'           => 'readonly',
            'required'           => 'required',
            'size'               => 'value',
            'src'                => 'value',
            'step'               => 'value',
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
        $this->assertContains('type="search"', $markup);
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }
}

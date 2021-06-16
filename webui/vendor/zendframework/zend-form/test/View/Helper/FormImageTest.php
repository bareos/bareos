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
use Zend\Form\View\Helper\FormImage as FormImageHelper;

class FormImageTest extends CommonTestCase
{
    public function setUp()
    {
        $this->helper = new FormImageHelper();
        parent::setUp();
    }

    public function testRaisesExceptionWhenNameIsNotPresentInElement()
    {
        $element = new Element();
        $element->setAttribute('src', 'foo.png');
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('name');
        $this->helper->render($element);
    }

    public function testRaisesExceptionWhenSrcIsNotPresentInElement()
    {
        $element = new Element('foo');
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('src');
        $this->helper->render($element);
    }

    public function testGeneratesImageInputTagWithElement()
    {
        $element = new Element('foo');
        $element->setAttribute('src', 'foo.png');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="image"', $markup);
        $this->assertContains('src="foo.png"', $markup);
    }

    public function testGeneratesImageInputTagRegardlessOfElementType()
    {
        $element = new Element('foo');
        $element->setAttribute('src', 'foo.png');
        $element->setAttribute('type', 'email');
        $markup  = $this->helper->render($element);
        $this->assertContains('<input ', $markup);
        $this->assertContains('type="image"', $markup);
        $this->assertContains('src="foo.png"', $markup);
    }

    public function validAttributes()
    {
        return [
            ['name', 'assertContains'],
            ['accept', 'assertNotContains'],
            ['alt', 'assertContains'],
            ['autocomplete', 'assertNotContains'],
            ['autofocus', 'assertContains'],
            ['checked', 'assertNotContains'],
            ['dirname', 'assertNotContains'],
            ['disabled', 'assertContains'],
            ['form', 'assertContains'],
            ['formaction', 'assertContains'],
            ['formenctype', 'assertContains'],
            ['formmethod', 'assertContains'],
            ['formnovalidate', 'assertContains'],
            ['formtarget', 'assertContains'],
            ['height', 'assertContains'],
            ['list', 'assertNotContains'],
            ['max', 'assertNotContains'],
            ['maxlength', 'assertNotContains'],
            ['min', 'assertNotContains'],
            ['multiple', 'assertNotContains'],
            ['pattern', 'assertNotContains'],
            ['placeholder', 'assertNotContains'],
            ['readonly', 'assertNotContains'],
            ['required', 'assertNotContains'],
            ['size', 'assertNotContains'],
            ['src', 'assertContains'],
            ['step', 'assertNotContains'],
            ['value', 'assertNotContains'],
            ['width', 'assertContains'],
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
            // Intentionally allowing fall-through
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
        $element->setAttribute('src', 'foo.png');
        $markup  = $this->helper->__invoke($element);
        $this->assertContains('<input', $markup);
        $this->assertContains('name="foo"', $markup);
        $this->assertContains('type="image"', $markup);
        $this->assertContains('src="foo.png"', $markup);
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }
}

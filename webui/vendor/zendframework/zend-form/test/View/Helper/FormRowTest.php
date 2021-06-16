<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\View\Helper;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Translator\TranslatorInterface;
use Zend\Form\Element;
use Zend\Form\Element\Captcha;
use Zend\Form\View\HelperConfig;
use Zend\Form\View\Helper\FormRow as FormRowHelper;
use Zend\View\Renderer\PhpRenderer;

class FormRowTest extends TestCase
{
    /**
     * @var FormRowHelper
     */
    protected $helper;

    /**
     * @var PhpRenderer
     */
    protected $renderer;

    public function setUp()
    {
        $this->helper = new FormRowHelper();

        $this->renderer = new PhpRenderer;
        $helpers = $this->renderer->getHelperPluginManager();
        $config  = new HelperConfig();
        $config->configureServiceManager($helpers);

        $this->helper->setView($this->renderer);
    }

    public function testCanGenerateLabel()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');
        $markup = $this->helper->render($element);
        $this->assertContains('>The value for foo:<', $markup);
        $this->assertContains('<label', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testCanCreateLabelValueBeforeInput()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');
        $this->helper->setLabelPosition('prepend');
        $markup = $this->helper->render($element);
        $this->assertContains('<label><span>The value for foo:</span><', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testCanCreateLabelValueAfterInput()
    {
        $element = new Element('foo');
        $element->setOptions([
            'label' => 'The value for foo:',
        ]);
        $this->helper->setLabelPosition('append');
        $markup = $this->helper->render($element);
        $this->assertContains('<label><input', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testCanOverrideLabelPosition()
    {
        $fooElement = new Element('foo');
        $fooElement->setOptions([
            'label'         => 'The value for foo:',
            'label_options' => [
                'label_position' => 'prepend'
            ],
        ]);

        $barElement = new Element('bar');
        $barElement->setOptions([
            'label' => 'The value for bar:',
        ]);

        $this->helper->setLabelPosition('append');

        $fooMarkup = $this->helper->render($fooElement);
        $this->assertContains('<label><span>The value for foo:</span><', $fooMarkup);
        $this->assertContains('</label>', $fooMarkup);

        $barMarkup = $this->helper->render($barElement);
        $this->assertContains('<label><', $barMarkup);
        $this->assertContains('<span>The value for bar:</span></label>', $barMarkup);
    }

    public function testCanRenderRowLabelAttributes()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');
        $element->setLabelAttributes(['class' => 'bar']);
        $this->helper->setLabelPosition('append');
        $markup = $this->helper->render($element);
        $this->assertContains("<label class=\"bar\">", $markup);
    }

    public function testCanCreateMarkupWithoutLabel()
    {
        $element = new Element('foo');
        $element->setAttribute('type', 'text');
        $markup = $this->helper->render($element);
        $this->assertRegexp('/<input name="foo" type="text"[^\/>]*\/?>/', $markup);
    }

    public function testIgnoreLabelForHidden()
    {
        $element = new Element\Hidden('foo');
        $element->setLabel('My label');
        $markup = $this->helper->render($element);
        $this->assertRegexp('/<input type="hidden" name="foo" value=""[^\/>]*\/?>/', $markup);
    }

    public function testCanHandleMultiCheckboxesCorrectly()
    {
        $options = [
            'This is the first label' => 'value1',
            'This is the second label' => 'value2',
            'This is the third label' => 'value3',
        ];

        $element = new Element\MultiCheckbox('foo');
        $element->setAttribute('type', 'multi_checkbox');
        $element->setAttribute('options', $options);
        $element->setLabel('This is a multi-checkbox');
        $markup = $this->helper->render($element);
        $this->assertContains("<fieldset>", $markup);
        $this->assertContains("<legend>", $markup);
        $this->assertContains("<label>", $markup);
    }


    public function testRenderAttributeId()
    {
        $element = new Element\Text('foo');
        $element->setAttribute('type', 'text');
        $element->setAttribute('id', 'textId');
        $element->setLabel('This is a text');
        $markup = $this->helper->render($element);
        $this->assertContains('<label for="textId">This is a text</label>', $markup);
        $this->assertContains('<input type="text" name="foo" id="textId"', $markup);
    }

    public function testCanRenderErrors()
    {
        $element  = new Element('foo');
        $element->setMessages([
            'First error message',
            'Second error message',
            'Third error message',
        ]);

        $markup = $this->helper->render($element);
        // @codingStandardsIgnoreStart
        $this->assertRegexp('#<ul>\s*<li>First error message</li>\s*<li>Second error message</li>\s*<li>Third error message</li>\s*</ul>#s', $markup);
        // @codingStandardsIgnoreEnd
    }

    public function testDoesNotRenderErrorsListIfSetToFalse()
    {
        $element  = new Element('foo');
        $element->setMessages([
            'First error message',
            'Second error message',
            'Third error message',
        ]);

        $markup = $this->helper->setRenderErrors(false)->render($element);
        $this->assertRegexp('/<input name="foo" class="input-error" type="text" [^\/>]*\/?>/', $markup);
    }

    public function testCanModifyDefaultErrorClass()
    {
        $element  = new Element('foo');
        $element->setMessages([
            'Error message'
        ]);

        $markup = $this->helper->setInputErrorClass('custom-error-class')->render($element);
        $this->assertRegexp('/<input name="foo" class="custom-error-class" type="text" [^\/>]*\/?>/', $markup);
    }

    public function testDoesNotOverrideClassesIfAlreadyPresentWhenThereAreErrors()
    {
        $element  = new Element('foo');
        $element->setMessages([
            'Error message'
        ]);
        $element->setAttribute('class', 'foo bar');

        $markup = $this->helper->setInputErrorClass('custom-error-class')->render($element);
        $this->assertRegexp(
            '/<input name="foo" class="foo\&\#x20\;bar\&\#x20\;custom-error-class" type="text" [^\/>]*\/?>/',
            $markup
        );
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }

    public function testLabelWillBeTranslated()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');

        $mockTranslator = $this->createMock('Zend\I18n\Translator\Translator');
        $mockTranslator->expects($this->any())
            ->method('translate')
            ->will($this->returnValue('translated content'));

        $this->helper->setTranslator($mockTranslator);
        $this->assertTrue($this->helper->hasTranslator());

        $markup = $this->helper->__invoke($element);
        $this->assertContains('>translated content<', $markup);
        $this->assertContains('<label', $markup);
        $this->assertContains('</label>', $markup);

        // Additional coverage when element's id is set
        $element->setAttribute('id', 'foo');
        $markup = $this->helper->__invoke($element);
        $this->assertContains('>translated content<', $markup);
        $this->assertContains('<label', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testTranslatorMethods()
    {
        $translatorMock = $this->createMock('Zend\I18n\Translator\Translator');
        $this->helper->setTranslator($translatorMock, 'foo');

        $this->assertEquals($translatorMock, $this->helper->getTranslator());
        $this->assertEquals('foo', $this->helper->getTranslatorTextDomain());
        $this->assertTrue($this->helper->hasTranslator());
        $this->assertTrue($this->helper->isTranslatorEnabled());

        $this->helper->setTranslatorEnabled(false);
        $this->assertFalse($this->helper->isTranslatorEnabled());
    }

    public function testLabelWillBeTranslatedOnceWithoutId()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');

        $mockTranslator = $this->createMock(TranslatorInterface::class);
        $mockTranslator->expects($this->exactly(1))
            ->method('translate')
            ->will($this->returnValue('translated content'));

        $this->helper->setTranslator($mockTranslator);
        $this->assertTrue($this->helper->hasTranslator());

        $markup = $this->helper->__invoke($element);
        $this->assertContains('>translated content<', $markup);
        $this->assertContains('<label', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testLabelWillBeTranslatedOnceWithId()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');
        $element->setAttribute('id', 'foo');

        $mockTranslator = $this->createMock(TranslatorInterface::class);
        $mockTranslator->expects($this->exactly(1))
            ->method('translate')
            ->will($this->returnValue('translated content'));

        $this->helper->setTranslator($mockTranslator);
        $this->assertTrue($this->helper->hasTranslator());

        $markup = $this->helper->__invoke($element);
        $this->assertContains('>translated content<', $markup);
        $this->assertContains('<label', $markup);
        $this->assertContains('</label>', $markup);
    }

    public function testSetLabelPositionInputNullRaisesException()
    {
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $this->helper->setLabelPosition(null);
    }

    public function testGetLabelPositionReturnsDefaultPrepend()
    {
        $labelPosition = $this->helper->getLabelPosition();
        $this->assertEquals('prepend', $labelPosition);
    }

    public function testGetLabelPositionReturnsAppend()
    {
        $this->helper->setLabelPosition('append');
        $labelPosition = $this->helper->getLabelPosition();
        $this->assertEquals('append', $labelPosition);
    }

    public function testGetRenderErrorsReturnsDefaultTrue()
    {
        $renderErrors = $this->helper->getRenderErrors();
        $this->assertTrue($renderErrors);
    }

    public function testGetRenderErrorsSetToFalse()
    {
        $this->helper->setRenderErrors(false);
        $renderErrors = $this->helper->getRenderErrors();
        $this->assertFalse($renderErrors);
    }

    public function testSetLabelAttributes()
    {
        $this->helper->setLabelAttributes(['foo', 'bar']);
        $this->assertEquals([0 => 'foo', 1 => 'bar'], $this->helper->getLabelAttributes());
    }

    public function testWhenUsingIdAndLabelBecomesEmptyRemoveSpan()
    {
        $element = new Element('foo');
        $element->setLabel('The value for foo:');

        $markup = $this->helper->__invoke($element);
        $this->assertContains('<span', $markup);
        $this->assertContains('</span>', $markup);

        $element->setAttribute('id', 'foo');

        $markup = $this->helper->__invoke($element);
        $this->assertNotContains('<span', $markup);
        $this->assertNotContains('</span>', $markup);
    }

    public function testShowErrorInMultiCheckbox()
    {
        $element = new Element\MultiCheckbox('hobby');
        $element->setLabel("Hobby");
        $element->setValueOptions([
            '0' => 'working',
            '1' => 'coding'
        ]);
        $element->setMessages([
            'Error message'
        ]);

        $markup = $this->helper->__invoke($element);
        $this->assertContains('<ul><li>Error message</li></ul>', $markup);
    }

    public function testShowErrorInRadio()
    {
        $element = new Element\Radio('direction');
        $element->setLabel("Direction");
        $element->setValueOptions([
            '0' => 'programming',
            '1' => 'design'
        ]);
        $element->setMessages([
            'Error message'
        ]);

        $markup = $this->helper->__invoke($element);
        $this->assertContains('<ul><li>Error message</li></ul>', $markup);
    }

    public function testErrorShowTwice()
    {
        $element = new  Element\Date('birth');
        $element->setFormat('Y-m-d');
        $element->setValue('2010.13');

        $validator = new \Zend\Validator\Date();
        $validator->isValid($element->getValue());
        $element->setMessages($validator->getMessages());

        $markup = $this->helper->__invoke($element);
        $this->assertEquals(
            2,
            count(explode("<ul><li>The input does not appear to be a valid date</li></ul>", $markup))
        );
    }

    public function testInvokeWithNoRenderErrors()
    {
        $mock = $this->getMockBuilder(get_class($this->helper))
            ->setMethods(['setRenderErrors'])
            ->getMock();
        $mock->expects($this->never())
                ->method('setRenderErrors');

        $mock->__invoke(new Element('foo'));
    }

    public function testInvokeWithRenderErrorsTrue()
    {
        $mock = $this->getMockBuilder(get_class($this->helper))
            ->setMethods(['setRenderErrors'])
            ->getMock();
        $mock->expects($this->once())
                ->method('setRenderErrors')
                ->with(true);

        $mock->__invoke(new Element('foo'), null, true);
    }

    public function testAppendLabelEvenIfElementHasId()
    {
        $element  = new Element('foo');
        $element->setAttribute('id', 'bar');
        $element->setLabel('Baz');

        $this->helper->setLabelPosition('append');
        $markup = $this->helper->render($element);
        $this->assertRegexp(
            '#^<input name="foo" id="bar" type="text" value=""\/?><label for="bar">Baz</label>$#',
            $markup
        );
    }

    public function testUsePartialView()
    {
        $element = new Element('fooname');
        $element->setLabel('foolabel');
        $partial = 'formrow-partial.phtml';

        $this->renderer->resolver()->addPath(__DIR__ . '/_templates');
        $markup = $this->helper->__invoke($element, null, null, $partial);
        $this->assertContains('fooname', $markup);
        $this->assertContains('foolabel', $markup);

        $this->assertSame($partial, $this->helper->getPartial());
    }

    public function testAssertButtonElementDoesNotRenderLabelTwice()
    {
        $element = new Element\Button('button');
        $element->setLabel('foo');

        $markup = $this->helper->render($element);
        $this->assertRegexp(
            '#^<button type="button" name="button" value=""\/?>foo</button>$#',
            $markup
        );
    }

    public function testAssertLabelHtmlEscapeIsOnByDefault()
    {
        $element = new Element('fooname');
        $escapeHelper = $this->renderer->getHelperPluginManager()->get('escapeHtml');

        $label = '<span>foo</span>';
        $element->setLabel($label);

        $markup = $this->helper->__invoke($element);

        $this->assertContains($escapeHelper->__invoke($label), $markup);
    }

    public function testCanDisableLabelHtmlEscape()
    {
        $label = '<span>foo</span>';
        $element = new Element('fooname');
        $element->setLabel($label);
        $element->setLabelOptions(['disable_html_escape' => true]);

        $markup = $this->helper->__invoke($element);

        $this->assertContains($label, $markup);
    }

    public function testCanSetLabelPositionBeforeInvoke()
    {
        $element = new Element('foo');

        $this->helper->setLabelPosition('append');
        $this->helper->__invoke($element);

        $this->assertSame('append', $this->helper->getLabelPosition());
    }

    /**
     * @covers Zend\Form\View\Helper\FormRow::render
     */
    public function testCanSetLabelPositionViaRender()
    {
        $element  = new Element('foo');
        $element->setAttribute('id', 'bar');
        $element->setLabel('Baz');

        $markup = $this->helper->render($element, 'append');
        $this->assertRegexp(
            '#^<input name="foo" id="bar" type="text" value=""\/?><label for="bar">Baz</label>$#',
            $markup
        );

        $markup = $this->helper->render($element, 'prepend');
        $this->assertRegexp(
            '#^<label for="bar">Baz</label><input name="foo" id="bar" type="text" value=""\/?>$#',
            $markup
        );
    }

    public function testSetLabelPositionViaRenderIsNotCached()
    {
        $labelPositionBeforeRender = $this->helper->getLabelPosition();
        $element = new Element('foo');

        $this->helper->render($element, 'append');
        $this->assertSame($labelPositionBeforeRender, $this->helper->getLabelPosition());

        $this->helper->render($element, 'prepend');
        $this->assertSame($labelPositionBeforeRender, $this->helper->getLabelPosition());
    }

    /**
     * @covers Zend\Form\View\Helper\FormRow::__invoke
     */
    public function testCanSetLabelPositionViaInvoke()
    {
        $element  = new Element('foo');
        $element->setAttribute('id', 'bar');
        $element->setLabel('Baz');

        $markup = $this->helper->__invoke($element, 'append');
        $this->assertRegexp(
            '#^<input name="foo" id="bar" type="text" value=""\/?><label for="bar">Baz</label>$#',
            $markup
        );

        $markup = $this->helper->__invoke($element, 'prepend');
        $this->assertRegexp(
            '#^<label for="bar">Baz</label><input name="foo" id="bar" type="text" value=""\/?>$#',
            $markup
        );
    }

    /**
     * @covers Zend\Form\View\Helper\FormRow::__invoke
     */
    public function testSetLabelPositionViaInvokeIsNotCached()
    {
        $labelPositionBeforeRender = $this->helper->getLabelPosition();
        $element = new Element('foo');

        $this->helper->__invoke($element, 'append');
        $this->assertSame($labelPositionBeforeRender, $this->helper->getLabelPosition());

        $this->helper->__invoke($element, 'prepend');
        $this->assertSame($labelPositionBeforeRender, $this->helper->getLabelPosition());
    }

    public function testLabelOptionAlwaysWrapDefaultsToFalse()
    {
        $element = new Element('foo');
        $this->assertEmpty($element->getLabelOption('always_wrap'));
    }

    public function testCanSetOptionToWrapElementInLabel()
    {
        $element = new Element('foo', [
            'label_options' => [
                'always_wrap' => true
            ]
        ]);
        $element->setAttribute('id', 'bar');
        $element->setLabel('baz');

        $markup = $this->helper->render($element);
        $this->assertRegexp(
            '#^<label><span>baz</span><input name="foo" id="bar" type="text" value=""\/?></label>$#',
            $markup
        );
    }

    /**
     * @group 7030
     */
    public function testWrapFieldsetAroundCaptchaWithLabel()
    {
        $this->assertRegexp(
            '#^<fieldset><legend>baz<\/legend>'
            . 'Please type this word backwards <b>[a-z0-9]{8}<\/b>'
            . '<input name="captcha&\#x5B;id&\#x5D;" type="hidden" value="[a-z0-9]{32}"\/?>'
            . '<input name="captcha&\#x5B;input&\#x5D;" type="text"\/?>'
            . '<\/fieldset>$#',
            $this->helper->render(new Captcha('captcha', [
                'captcha' => ['class' => 'dumb'],
                'label' => 'baz'
            ]))
        );
    }
}

<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\View\Helper;

use Zend\Form\Element\DateSelect;
use Zend\Form\View\Helper\FormDateSelect as FormDateSelectHelper;

class FormDateSelectTest extends CommonTestCase
{
    public function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->helper = new FormDateSelectHelper();
        parent::setUp();
    }

    public function testRaisesExceptionWhenNameIsNotPresentInElement()
    {
        $element = new DateSelect();
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('name');
        $this->helper->render($element);
    }

    public function testGeneratesThreeSelectsWithElement()
    {
        $element = new DateSelect('foo');
        $markup  = $this->helper->render($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
    }

    public function testCanGenerateSelectsWithEmptyOption()
    {
        $element = new DateSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $markup  = $this->helper->render($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
        $this->assertContains('<option value=""></option>', $markup);
    }

    public function testCanDisableDelimiters()
    {
        $element = new DateSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $element->setShouldRenderDelimiters(false);
        $markup = $this->helper->render($element);

        // If it contains two consecutive selects this means that no delimiters
        // are inserted
        $this->assertContains('</select><select', $markup);
    }

    public function testCanRenderTextDelimiters()
    {
        $element = new DateSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $element->setShouldRenderDelimiters(true);
        $markup = $this->helper->__invoke($element, \IntlDateFormatter::LONG, 'pt_BR');

        // pattern === "d 'de' MMMM 'de' y"
        $this->assertStringMatchesFormat('%a de %a de %a', $markup);
    }

    public function testInvokeProxiesToRender()
    {
        $element = new DateSelect('foo');
        $markup  = $this->helper->__invoke($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }

    public function testDayElementValueOptions()
    {
        $element = new DateSelect('foo');
        $this->helper->render($element);
        $this->assertEquals(31, count($element->getDayElement()->getValueOptions()));
    }

    /**
     * @group 6656
     */
    public function testGetElements()
    {
        $element = new DateSelect('foo');
        $this->helper->render($element);
        $elements = $element->getElements();
        $this->assertEquals(3, count($elements));

        foreach ($elements as $subElement) {
            $this->assertInstanceOf('Zend\Form\Element\Select', $subElement);
        }

        $this->assertEquals(31, count($elements[0]->getValueOptions()));
    }
}

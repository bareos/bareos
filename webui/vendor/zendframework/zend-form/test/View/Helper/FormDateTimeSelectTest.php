<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\View\Helper;

use Zend\Form\Element\DateTimeSelect;
use Zend\Form\View\Helper\FormDateTimeSelect as FormDateTimeSelectHelper;

class FormDateTimeSelectTest extends CommonTestCase
{
    public function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->helper = new FormDateTimeSelectHelper();
        parent::setUp();
    }

    public function testRaisesExceptionWhenNameIsNotPresentInElement()
    {
        $element = new DateTimeSelect();
        $this->expectException('Zend\Form\Exception\DomainException');
        $this->expectExceptionMessage('name');
        $this->helper->render($element);
    }

    public function testGeneratesFiveSelectsWithElementByDefault()
    {
        $element = new DateTimeSelect('foo');
        $markup  = $this->helper->render($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
        $this->assertContains('<select name="hour"', $markup);
        $this->assertContains('<select name="minute"', $markup);
        $this->assertNotContains('<select name="second"', $markup);
    }

    public function testGeneratesSecondSelectIfAskedByElement()
    {
        $element = new DateTimeSelect('foo');
        $element->setShouldShowSeconds(true);
        $markup  = $this->helper->render($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
        $this->assertContains('<select name="hour"', $markup);
        $this->assertContains('<select name="minute"', $markup);
        $this->assertContains('<select name="second"', $markup);
    }

    public function testCanGenerateSelectsWithEmptyOption()
    {
        $element = new DateTimeSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $markup  = $this->helper->render($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
        $this->assertContains('<select name="hour"', $markup);
        $this->assertContains('<select name="minute"', $markup);
        $this->assertContains('<option value=""></option>', $markup);
    }

    public function testCanDisableDelimiters()
    {
        $element = new DateTimeSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $element->setShouldRenderDelimiters(false);
        $markup = $this->helper->render($element);

        // If it contains two consecutive selects this means that no delimiters
        // are inserted
        $this->assertContains('</select><select', $markup);
    }

    public function testCanRenderTextDelimiters()
    {
        $element = new DateTimeSelect('foo');
        $element->setShouldCreateEmptyOption(true);
        $element->setShouldRenderDelimiters(true);
        $element->setShouldShowSeconds(true);
        $markup = $this->helper->__invoke($element, \IntlDateFormatter::LONG, \IntlDateFormatter::LONG, 'pt_BR');

        // pattern === "d 'de' MMMM 'de' y HH'h'mm'min'ss's'"
        $this->assertStringMatchesFormat('%a de %a de %a %ah%amin%as%a', $markup);
    }

    public function testInvokeProxiesToRender()
    {
        $element = new DateTimeSelect('foo');
        $markup  = $this->helper->__invoke($element);
        $this->assertContains('<select name="day"', $markup);
        $this->assertContains('<select name="month"', $markup);
        $this->assertContains('<select name="year"', $markup);
        $this->assertContains('<select name="hour"', $markup);
        $this->assertContains('<select name="minute"', $markup);
    }

    public function testInvokeWithNoElementChainsHelper()
    {
        $this->assertSame($this->helper, $this->helper->__invoke());
    }

    public function testNoMinutesDelimiterIfSecondsNotShown()
    {
        $element  = new DateTimeSelect('foo');
        $element->setValue([
            'year'   => '2012',
            'month'  => '09',
            'day'    => '24',
            'hour'   => '03',
            'minute' => '04',
            'second' => '59',
        ]);

        $element->setShouldShowSeconds(false);
        $element->shouldRenderDelimiters(true);
        $markup  = $this->helper->__invoke($element);

        // the last $markup char should be the '>' of the minutes  html select
        // closing tag and not the delimiter
        $this->assertEquals('>', substr($markup, -1));
    }
}

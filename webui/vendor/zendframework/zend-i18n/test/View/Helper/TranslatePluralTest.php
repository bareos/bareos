<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\View\Helper;

use PHPUnit\Framework\TestCase;
use Zend\I18n\View\Helper\TranslatePlural as TranslatePluralHelper;
use Zend\I18n\Translator\Translator;

/**
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class TranslatePluralTest extends TestCase
{
    /**
     * @var TranslatePluralHelper
     */
    public $helper;

    /**
     * Sets up the fixture, for example, open a network connection.
     * This method is called before a test is executed.
     *
     * @return void
     */
    protected function setUp()
    {
        $this->helper = new TranslatePluralHelper();
    }

    /**
     * Tears down the fixture, for example, close a network connection.
     * This method is called after a test is executed.
     *
     * @return void
     */
    protected function tearDown()
    {
        unset($this->helper);
    }

    public function testInvokingWithoutTranslatorWillRaiseException()
    {
        $this->expectException('Zend\I18n\Exception\RuntimeException');
        $this->helper->__invoke('singular', 'plural', 1);
    }

    public function testDefaultInvokeArguments()
    {
        $singularInput = 'singular';
        $pluralInput   = 'plural';
        $numberInput   = 1;
        $expected      = 'translated';

        $translatorMock = $this->prophesize(Translator::class);
        $translatorMock->translatePlural($singularInput, $pluralInput, $numberInput, 'default', null)
            ->willReturn($expected)
            ->shouldBeCalledTimes(1);

        $this->helper->setTranslator($translatorMock->reveal());

        $this->assertEquals($expected, $this->helper->__invoke($singularInput, $pluralInput, $numberInput));
    }

    public function testCustomInvokeArguments()
    {
        $singularInput = 'singular';
        $pluralInput   = 'plural';
        $numberInput   = 1;
        $expected      = 'translated';
        $textDomain    = 'textDomain';
        $locale        = 'en_US';

        $translatorMock = $this->prophesize(Translator::class);
        $translatorMock->translatePlural($singularInput, $pluralInput, $numberInput, $textDomain, $locale)
            ->willReturn($expected)
            ->shouldBeCalledTimes(1);

        $this->helper->setTranslator($translatorMock->reveal());

        $this->assertEquals($expected, $this->helper->__invoke(
            $singularInput,
            $pluralInput,
            $numberInput,
            $textDomain,
            $locale
        ));
    }
}

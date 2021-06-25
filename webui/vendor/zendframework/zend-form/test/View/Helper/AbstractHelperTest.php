<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\View\Helper;

use Zend\Escaper\Escaper;
use Zend\Form\Exception\InvalidArgumentException;
use Zend\Form\View\Helper\AbstractHelper;
use Zend\I18n\Translator\Translator;

/**
 * Tests for {@see \Zend\Form\View\Helper\AbstractHelper}
 *
 * @covers \Zend\Form\View\Helper\AbstractHelper
 */
class AbstractHelperTest extends CommonTestCase
{
    public function setUp()
    {
        $this->helper = $this->getMockForAbstractClass('Zend\Form\View\Helper\AbstractHelper');
        parent::setUp();
    }

    /**
     * @group 5991
     */
    public function testWillEscapeValueAttributeValuesCorrectly()
    {
        $this->assertSame(
            'data-value="breaking&#x20;your&#x20;HTML&#x20;like&#x20;a&#x20;boss&#x21;&#x20;&#x5C;"',
            $this->helper->createAttributesString(['data-value' => 'breaking your HTML like a boss! \\'])
        );
    }

    public function testWillEncodeValueAttributeValuesCorrectly()
    {
        $escaper = new Escaper('iso-8859-1');

        $this->helper->setEncoding('iso-8859-1');

        $this->assertSame(
            'data-value="' . $escaper->escapeHtmlAttr('Título') . '"',
            $this->helper->createAttributesString(['data-value' => 'Título'])
        );
    }

    public function testWillNotEncodeValueAttributeValuesCorrectly()
    {
        $escaper = new Escaper('iso-8859-1');

        $this->assertNotSame(
            'data-value="' . $escaper->escapeHtmlAttr('Título') . '"',
            $this->helper->createAttributesString(['data-value' => 'Título'])
        );
    }

    public function addAttributesData()
    {
        return [
            'valid'                => ['valid', 'valid="value"'],
            'valid-prefix'         => ['px-custom', 'px-custom="value"'],
            'xml-ns'               => ['xlink:custom', 'xlink:custom="value"'],
            'invalid-slash'        => ['attr/', null, true],
            'invalid-double-quote' => ['attr"', null, true],
            'invalid-quote'        => ['attr\'', null, true],
            'invalid-gt'           => ['attr>', null, true],
            'invalid-equals'       => ['attr=value', null, true],
            'invalid-space'        => ['at tr', null, true],
            'invalid-newline'      => ["at\ntr", null, true],
            'invalid-tab'          => ["at\ttr", null, true],
            'invalid-formfeed'     => ["at\ftr", null, true],
        ];
    }

    /**
     * @dataProvider addAttributesData
     */
    public function testWillIncludeAdditionalAttributes($attribute, $expected = null, $exception = null)
    {
        if ($exception) {
            $this->expectException(InvalidArgumentException::class);
        }

        $this->helper->addValidAttribute($attribute);

        $this->assertSame(
            $expected,
            $this->helper->createAttributesString([$attribute => 'value'])
        );
    }

    public function addAttributesPrefixData()
    {
        return [
            'valid'                => ['v-', 'v-attr="value"'],
            'valid-dash'           => ['custom-', 'custom-attr="value"'],
            'xml-ns'               => ['xlink:', 'xlink:attr="value"'],
            'valid-nodash'         => ['abc', 'abcattr="value"'],
            'invalid-slash'        => ['custom/', null, true],
            'invalid-double-quote' => ['custom"', null, true],
            'invalid-quote'        => ['custom\'', null, true],
            'invalid-gt'           => ['custom>', null, true],
            'invalid-equals'       => ['custom=', null, true],
            'invalid-space'        => ['custom ', null, true],
            'invalid-newline'      => ["cus\ntom", null, true],
            'invalid-tab'          => ["cus\ttom", null, true],
            'invalid-formfeed'     => ["cus\ftom", null, true],
        ];
    }

    /**
     * @dataProvider addAttributesPrefixData
     */
    public function testWillIncludeAdditionalAttributesByPrefix($prefix, $expected = null, $exception = null)
    {
        if ($exception) {
            $this->expectException(InvalidArgumentException::class);
        }

        $this->helper->addValidAttributePrefix($prefix);

        $this->assertSame(
            $expected,
            $this->helper->createAttributesString([$prefix . 'attr' => 'value'])
        );
    }

    public function testWillTranslateAttributeValuesCorrectly()
    {
        $translator = self::getMockBuilder(Translator::class)
            ->disableOriginalConstructor()
            ->setMethods(['translate'])
            ->getMock();

        $translator
            ->expects(self::exactly(2))
            ->method('translate')
            ->with(
                self::equalTo('Welcome'),
                self::equalTo('view-helper-text-domain')
            )
            ->willReturn('Willkommen');

        $this->helper
            ->addTranslatableAttribute('data-translate-me')
            ->addTranslatableAttributePrefix('data-translatable-')
            ->setTranslatorEnabled(true)
            ->setTranslator(
                $translator,
                'view-helper-text-domain'
            );

        $this->assertSame(
            'data-translate-me="Willkommen"',
            $this->helper->createAttributesString(['data-translate-me' => 'Welcome'])
        );

        $this->assertSame(
            'data-translatable-welcome="Willkommen"',
            $this->helper->createAttributesString(['data-translatable-welcome' => 'Welcome'])
        );

        $this->assertSame(
            'class="Welcome"',
            $this->helper->createAttributesString(['class' => 'Welcome'])
        );
    }

    public function testWillTranslateDefaultAttributeValuesCorrectly()
    {
        $translator = self::getMockBuilder(Translator::class)
            ->disableOriginalConstructor()
            ->setMethods(['translate'])
            ->getMock();

        $translator
            ->expects(self::exactly(2))
            ->method('translate')
            ->with(
                self::equalTo('Welcome'),
                self::equalTo('view-helper-text-domain')
            )
            ->willReturn('Willkommen');

        AbstractHelper::addDefaultTranslatableAttribute('data-translate-me');
        AbstractHelper::addDefaultTranslatableAttributePrefix('data-translatable-');

        $this->helper
            ->setTranslatorEnabled(true)
            ->setTranslator(
                $translator,
                'view-helper-text-domain'
            );

        $this->assertSame(
            'data-translate-me="Willkommen"',
            $this->helper->createAttributesString(['data-translate-me' => 'Welcome'])
        );

        $this->assertSame(
            'data-translatable-welcome="Willkommen"',
            $this->helper->createAttributesString(['data-translatable-welcome' => 'Welcome'])
        );

        $this->assertSame(
            'class="Welcome"',
            $this->helper->createAttributesString(['class' => 'Welcome'])
        );
    }

    public function testWillInsulateAgainstBadAttributes()
    {
        $this->assertSame(
            'data-value=""',
            $this->helper->createAttributesString(['data-value' => "\xc3\x28"])
        );
    }
}

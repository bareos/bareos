<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Translator;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Translator\Translator;

/**
 * @requires PHP 5.4
 */
class TranslatorAwareTraitTest extends TestCase
{
    public function testSetTranslator()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertAttributeEquals(null, 'translator', $object);

        $translator = new Translator;

        $object->setTranslator($translator);

        $this->assertAttributeEquals($translator, 'translator', $object);
    }

    public function testSetTranslatorAndTextDomain()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertAttributeEquals(null, 'translator', $object);
        $this->assertAttributeEquals('default', 'translatorTextDomain', $object);

        $translator = new Translator;
        $textDomain = 'domain';

        $object->setTranslator($translator, $textDomain);

        $this->assertAttributeEquals($translator, 'translator', $object);
        $this->assertAttributeEquals($textDomain, 'translatorTextDomain', $object);
    }

    public function testGetTranslator()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertNull($object->getTranslator());

        $translator = new Translator;

        $object->setTranslator($translator);

        $this->assertEquals($translator, $object->getTranslator());
    }

    public function testHasTranslator()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertFalse($object->hasTranslator());

        $translator = new Translator;

        $object->setTranslator($translator);

        $this->assertTrue($object->hasTranslator());
    }

    public function testSetTranslatorEnabled()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertAttributeEquals(true, 'translatorEnabled', $object);

        $enabled = false;

        $object->setTranslatorEnabled($enabled);

        $this->assertAttributeEquals($enabled, 'translatorEnabled', $object);

        $object->setTranslatorEnabled();

        $this->assertAttributeEquals(true, 'translatorEnabled', $object);
    }

    public function testIsTranslatorEnabled()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertTrue($object->isTranslatorEnabled());

        $object->setTranslatorEnabled(false);

        $this->assertFalse($object->isTranslatorEnabled());
    }

    public function testSetTranslatorTextDomain()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertAttributeEquals('default', 'translatorTextDomain', $object);

        $textDomain = 'domain';

        $object->setTranslatorTextDomain($textDomain);

        $this->assertAttributeEquals($textDomain, 'translatorTextDomain', $object);
    }

    public function testGetTranslatorTextDomain()
    {
        $object = $this->getObjectForTrait('\Zend\I18n\Translator\TranslatorAwareTrait');

        $this->assertEquals('default', $object->getTranslatorTextDomain());

        $textDomain = 'domain';

        $object->setTranslatorTextDomain($textDomain);

        $this->assertEquals($textDomain, $object->getTranslatorTextDomain());
    }
}

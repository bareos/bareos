<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Validator;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Validator\IsInt as IsIntValidator;
use Locale;
use Zend\Validator\Exception;

/**
 * @group      Zend_Validator
 */
class IsIntTest extends TestCase
{
    /**
     * @var Int
     */
    protected $validator;

    /**
     * @var string
     */
    protected $locale;

    protected function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->locale    = Locale::getDefault();
        $this->validator = new IsIntValidator();
    }

    protected function tearDown()
    {
        if (extension_loaded('intl')) {
            Locale::setDefault($this->locale);
        }
    }

    public function intDataProvider()
    {
        return [
            [1.00,         true],
            [0.00,         true],
            [0.01,         false],
            [-0.1,         false],
            [-1,           true],
            ['10',         true],
            [1,            true],
            ['not an int', false],
            [true,         false],
            [false,        false],
        ];
    }

    /**
     * Ensures that the validator follows expected behavior
     *
     * @dataProvider intDataProvider()
     * @return void
     */
    public function testBasic($intVal, $expected)
    {
        $this->validator->setLocale('en');
        $this->assertEquals($expected, $this->validator->isValid($intVal));
    }

    /**
     * Ensures that getMessages() returns expected default value
     *
     * @return void
     */
    public function testGetMessages()
    {
        $this->assertEquals([], $this->validator->getMessages());
    }

    /**
     * Ensures that set/getLocale() works
     */
    public function testSettingLocales()
    {
        $this->validator->setLocale('de');
        $this->assertEquals('de', $this->validator->getLocale());
        $this->assertEquals(false, $this->validator->isValid('10 000'));
        $this->assertEquals(true, $this->validator->isValid('10.000'));
    }

    /**
     * @ZF-4352
     */
    public function testNonStringValidation()
    {
        $this->assertFalse($this->validator->isValid([1 => 1]));
    }

    /**
     * @ZF-7489
     */
    public function testUsingApplicationLocale()
    {
        Locale::setDefault('de');
        $valid = new IsIntValidator();
        $this->assertTrue($valid->isValid('10.000'));
    }

    /**
     * @ZF-7703
     */
    public function testLocaleDetectsNoEnglishLocaleOnOtherSetLocale()
    {
        Locale::setDefault('de');
        $valid = new IsIntValidator();
        $this->assertTrue($valid->isValid(1200));
        $this->assertFalse($valid->isValid('1,200'));
    }

    public function testEqualsMessageTemplates()
    {
        $validator = $this->validator;
        $this->assertAttributeEquals(
            $validator->getOption('messageTemplates'),
            'messageTemplates',
            $validator
        );
    }

    public function testGetStrict()
    {
        $this->assertFalse(
            $this->validator->getStrict()
        );

        $this->validator->setStrict(true);
        $this->assertTrue(
            $this->validator->getStrict()
        );
    }

    /**
     * @return array
     */
    public function setStrictInvalidParameterDataProvider()
    {
        return [
            [null],
            ['true'],
            ['1'],
            ['1.0'],
            ['false'],
            ['0'],
            ['0.0'],
        ];
    }

    /**
     * @dataProvider setStrictInvalidParameterDataProvider
     * @param mixed $strict
     */
    public function testSetStrictThrowsInvalidArgumentException($strict)
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->validator->setStrict($strict);
    }

    /**
     * @return array
     */
    public function strictIntDataProvider()
    {
        return [
            [1,            true],
            [0,            true],
            [1.00,         false],
            [0.00,         false],
            [0.01,         false],
            [-0.1,         false],
            [-1,           true],
            ['10',         false],
            ['1',          false],
            ['not an int', false],
            [true,         false],
            [false,        false],
        ];
    }

    /**
     * @dataProvider strictIntDataProvider
     * @param mixed $intVal
     * @param bool $expected
     * @return void
     */
    public function testStrictComparison($intVal, $expected)
    {
        $this->validator->setLocale('en');
        $this->validator->setStrict(true);

        $this->assertSame($expected, $this->validator->isValid($intVal));
    }
}

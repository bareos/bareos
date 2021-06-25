<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Validator;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Validator\PostCode as PostCodeValidator;

/**
 * @group      Zend_Validator
 */
class PostCodeTest extends TestCase
{
    /**
     * @var  PostCode
     */
    protected $validator;

    /**
     * Creates a new Zend\PostCode object for each test method
     *
     * @return void
     */
    protected function setUp()
    {
        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->validator = new PostCodeValidator(['locale' => 'de_AT']);
    }

    /**
     * @dataProvider UKPostCodesDataProvider
     * @group #7250
     * @group #7264
     */
    public function testUKBasic($postCode, $expected)
    {
        $uk_validator = new PostCodeValidator(['locale' => 'en_GB']);
        $this->assertSame($expected, $uk_validator->isValid($postCode));
    }

    public function UKPostCodesDataProvider()
    {
        return [
            ['CA3 5JQ', true],
            ['GL15 2GB', true],
            ['GL152GB', true],
            ['ECA32 6JQ', false],
            ['se5 0eg', false],
            ['SE5 0EG', true],
            ['ECA3 5JQ', false],
            ['WC2H 7LTa', false],
            ['WC2H 7LTA', false],
        ];
    }

    public function postCodesDataProvider()
    {
        return [
            ['2292',    true],
            ['1000',    true],
            ['0000',    true],
            ['12345',   false],
            [1234,      true],
            [9821,      true],
            ['21A4',    false],
            ['ABCD',    false],
            [true,      false],
            ['AT-2292', false],
            [1.56,      false],
        ];
    }

    /**
     * Ensures that the validator follows expected behavior
     *
     * @dataProvider postCodesDataProvider
     * @return void
     */
    public function testBasic($postCode, $expected)
    {
        $this->assertEquals($expected, $this->validator->isValid($postCode));
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
     * Ensures that a region is available
     */
    public function testSettingLocalesWithoutRegion()
    {
        $this->expectException('Zend\Validator\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('Locale must contain a region');
        $this->validator->setLocale('de')->isValid('1000');
    }

    /**
     * Ensures that the region contains postal codes
     */
    public function testSettingLocalesWithoutPostalCodes()
    {
        $this->expectException('Zend\Validator\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('A postcode-format string has to be given for validation');
        $this->validator->setLocale('gez_ER')->isValid('1000');
    }

    /**
     * Ensures locales can be retrieved
     */
    public function testGettingLocale()
    {
        $this->assertEquals('de_AT', $this->validator->getLocale());
    }

    /**
     * Ensures format can be set and retrieved
     */
    public function testSetGetFormat()
    {
        $this->validator->setFormat('\d{1}');
        $this->assertEquals('\d{1}', $this->validator->getFormat());
    }

    public function testSetGetFormatThrowsExceptionOnNullFormat()
    {
        $this->expectException('Zend\Validator\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('A postcode-format string has to be given');
        $this->validator->setLocale(null)->setFormat(null)->isValid('1000');
    }

    public function testSetGetFormatThrowsExceptionOnEmptyFormat()
    {
        $this->expectException('Zend\Validator\Exception\InvalidArgumentException');
        $this->expectExceptionMessage('A postcode-format string has to be given');
        $this->validator->setLocale(null)->setFormat('')->isValid('1000');
    }

    /**
     * @group ZF-9212
     */
    public function testErrorMessageText()
    {
        $this->assertFalse($this->validator->isValid('hello'));
        $message = $this->validator->getMessages();
        $this->assertContains('not appear to be a postal code', $message['postcodeNoMatch']);
    }

     /**
     * Test service class with invalid validation
     *
     * @group ZF2-44
     */
    public function testServiceClass()
    {
        $params = (object)[
            'serviceTrue'   => null,
            'serviceFalse'  => null,
        ];

        $serviceTrue  = static function ($value) use ($params) {
            $params->serviceTrue = $value;
            return true;
        };

        $serviceFalse = static function ($value) use ($params) {
            $params->serviceFalse = $value;
            return false;
        };

        $this->assertEquals(null, $this->validator->getService());


        $this->validator->setService($serviceTrue);
        $this->assertEquals($this->validator->getService(), $serviceTrue);
        $this->assertTrue($this->validator->isValid('2292'));
        $this->assertEquals($params->serviceTrue, '2292');


        $this->validator->setService($serviceFalse);
        $this->assertEquals($this->validator->getService(), $serviceFalse);
        $this->assertFalse($this->validator->isValid('hello'));
        $this->assertEquals($params->serviceFalse, 'hello');

        $message = $this->validator->getMessages();
        $this->assertContains('not appear to be a postal code', $message['postcodeService']);
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

    /**
     * Post codes are provided by French government official post code database
     * https://www.data.gouv.fr/fr/datasets/base-officielle-des-codes-postaux/
     */
    public function testFrPostCodes()
    {
        $validator = $this->validator;
        $validator->setLocale('fr_FR');

        $this->assertTrue($validator->isValid('13100')); // AIX EN PROVENCE
        $this->assertTrue($validator->isValid('97439')); // STE ROSE
        $this->assertTrue($validator->isValid('98790')); // MAHETIKA
        $this->assertFalse($validator->isValid('00000')); // Post codes starting with 00 don't exist
        $this->assertFalse($validator->isValid('96000')); // Post codes starting with 96 don't exist
        $this->assertFalse($validator->isValid('99000')); // Post codes starting with 99 don't exist
    }

    /**
     * Post codes are provided by Norway Mail database
     * http://www.bring.no/hele-bring/produkter-og-tjenester/brev-og-postreklame/andre-tjenester/postnummertabeller
     */
    public function testNoPostCodes()
    {
        $validator = $this->validator;
        $validator->setLocale('en_NO');

        $this->assertTrue($validator->isValid('0301')); // OSLO
        $this->assertTrue($validator->isValid('9910')); // BJØRNEVATN
        $this->assertFalse($validator->isValid('0000')); // Postal code 0000
    }

    /**
     * Postal codes in Latvia are 4 digit numeric and use a mandatory ISO 3166-1 alpha-2 country code (LV) in front,
     * i.e. the format is “LV-NNNN”.
     * To prevent BC break LV- prefix is optional
     * https://en.wikipedia.org/wiki/Postal_codes_in_Latvia
     */
    public function testLvPostCodes()
    {
        $validator = $this->validator;
        $validator->setLocale('en_LV');

        $this->assertTrue($validator->isValid('LV-0000'));
        $this->assertTrue($validator->isValid('0000'));
        $this->assertFalse($validator->isValid('ABCD'));
        $this->assertFalse($validator->isValid('LV-ABCD'));
    }

    public function liPostCode()
    {
        yield 'Nendeln' => [9485];
        yield 'Schaanwald' => [9486];
        yield 'Gamprin-Bendern' => [9487];
        yield 'Schellenberg' => [9488];
        yield 'Vaduz-9489' => [9489];
        yield 'Vaduz-9490' => [9490];
        yield 'Ruggell' => [9491];
        yield 'Eschen' => [9492];
        yield 'Mauren' => [9493];
        yield 'Schaan' => [9494];
        yield 'Triesen' => [9495];
        yield 'Balzers' => [9496];
        yield 'Triesenberg' => [9497];
        yield 'Planken' => [9498];
    }

    /**
     * @dataProvider liPostCode
     * @param int $postCode
     */
    public function testLiPostCodes($postCode)
    {
        $validator = $this->validator;
        $validator->setLocale('de_LI');

        $this->assertTrue($validator->isValid($postCode));
    }
}

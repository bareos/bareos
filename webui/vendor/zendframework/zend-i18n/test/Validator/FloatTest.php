<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\I18n\Validator;

use PHPUnit\Framework\TestCase;
use Zend\I18n\Validator\Float as FloatValidator;
use Locale;

/**
 * @group      Zend_Validator
 */
class FloatTest extends TestCase
{
    /**
     * @var FloatValidator
     */
    protected $validator;

    /**
     * @var string
     */
    protected $locale;

    protected function setUp()
    {
        if (PHP_VERSION_ID >= 70000) {
            $this->markTestSkipped('Cannot test Float validator under PHP 7; reserved keyword');
        }

        if (! extension_loaded('intl')) {
            $this->markTestSkipped('ext/intl not enabled');
        }

        $this->locale = Locale::getDefault();
    }

    protected function tearDown()
    {
        if (extension_loaded('intl')) {
            Locale::setDefault($this->locale);
        }
    }

    public function testConstructorRaisesDeprecationNotice()
    {
        $this->expectException('PHPUnit_Framework_Error_Deprecated');
        new FloatValidator();
    }
}

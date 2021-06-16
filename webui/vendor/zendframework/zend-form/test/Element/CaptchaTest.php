<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Element;

use PHPUnit\Framework\TestCase;
use ArrayIterator;
use ArrayObject;
use Zend\Captcha;
use Zend\Form\Element\Captcha as CaptchaElement;
use Zend\Form\Factory;
use ZendTest\Form\TestAsset;

class CaptchaTest extends TestCase
{
    public function testCaptchaIsUndefinedByDefault()
    {
        $element = new CaptchaElement();
        $this->assertNull($element->getCaptcha());
    }

    public function testCaptchaIsMutable()
    {
        $element = new CaptchaElement();

        // by instance
        $captcha = new Captcha\Dumb();
        $element->setCaptcha($captcha);
        $this->assertSame($captcha, $element->getCaptcha());

        // by array
        $captcha = [
            'class'   => 'dumb',
        ];
        $element->setCaptcha($captcha);
        $this->assertInstanceOf('Zend\Captcha\Dumb', $element->getCaptcha());

        // by traversable
        $captcha = new ArrayObject([
            'class'   => 'dumb',
        ]);
        $element->setCaptcha($captcha);
        $this->assertInstanceOf('Zend\Captcha\Dumb', $element->getCaptcha());
    }

    public function testCaptchaWithNullRaisesException()
    {
        $element = new CaptchaElement();
        $this->expectException('Zend\Form\Exception\InvalidArgumentException');
        $element->setCaptcha(null);
    }

    public function testSettingCaptchaSetsCaptchaAttribute()
    {
        $element = new CaptchaElement();
        $captcha = new Captcha\Dumb();
        $element->setCaptcha($captcha);
        $this->assertSame($captcha, $element->getCaptcha());
    }

    public function testCreatingCaptchaElementViaFormFactoryWillCreateCaptcha()
    {
        $factory = new Factory();
        $element = $factory->createElement([
            'type'       => 'Zend\Form\Element\Captcha',
            'name'       => 'foo',
            'options'    => [
                'captcha' => [
                    'class'   => 'dumb'
                ]
            ]
        ]);
        $this->assertInstanceOf('Zend\Form\Element\Captcha', $element);
        $captcha = $element->getCaptcha();
        $this->assertInstanceOf('Zend\Captcha\Dumb', $captcha);
    }

    public function testProvidesInputSpecificationThatIncludesCaptchaAsValidator()
    {
        $element = new CaptchaElement();
        $captcha = new Captcha\Dumb();
        $element->setCaptcha($captcha);

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);
        $test = array_shift($inputSpec['validators']);
        $this->assertSame($captcha, $test);
    }

    /**
     * @group 3446
     */
    public function testAllowsPassingTraversableOptionsToConstructor()
    {
        $options = new TestAsset\IteratorAggregate(new ArrayIterator([
            'captcha' => [
                'class'   => 'dumb',
            ],
        ]));
        $element = new CaptchaElement('captcha', $options);
        $captcha = $element->getCaptcha();
        $this->assertInstanceOf('Zend\Captcha\Dumb', $captcha);
    }
}

<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\View\Helper\Captcha;

use Zend\Captcha\ReCaptcha;
use Zend\Form\Element\Captcha as CaptchaElement;
use Zend\Form\View\Helper\Captcha\ReCaptcha as ReCaptchaHelper;
use ZendTest\Form\View\Helper\CommonTestCase;

class ReCaptchaTest extends CommonTestCase
{
    public function setUp()
    {
        if (! getenv('TESTS_ZEND_FORM_RECAPTCHA_SUPPORT')) {
            $this->markTestSkipped('Enable TESTS_ZEND_FORM_RECAPTCHA_SUPPORT to test PDF render');
        }

        if (! class_exists(ReCaptcha::class)) {
            $this->markTestSkipped(
                'zend-captcha-related tests are skipped until the component '
                . 'is forwards-compatible with zend-servicemanager v3'
            );
        }

        $this->helper  = new ReCaptchaHelper();
        $this->captcha = new ReCaptcha();
        $this->captcha->setPubKey(getenv('TESTS_ZEND_FORM_RECAPTCHA_PUBLIC_KEY'));
        $this->captcha->setPrivKey(getenv('TESTS_ZEND_FORM_RECAPTCHA_PRIVATE_KEY'));
        parent::setUp();
    }

    public function getElement()
    {
        $element = new CaptchaElement('foo');
        $element->setCaptcha($this->captcha);
        return $element;
    }

    public function testMissingCaptchaAttributeThrowsDomainException()
    {
        $element = new CaptchaElement('foo');

        $this->expectException('Zend\Form\Exception\DomainException');
        $this->helper->render($element);
    }

    public function testRendersHiddenInputWhenNameIsNotRecaptchaDefault()
    {
        $element = $this->getElement();
        $markup  = $this->helper->render($element);
        $this->assertContains('type="hidden"', $markup);
        $this->assertContains('value="g-recaptcha-response"', $markup);
    }

    public function testDoesNotRenderHiddenInputWhenNameIsRecaptchaDefault()
    {
        $element = $this->getElement();
        $element->setName('g-recaptcha-response');
        $markup  = $this->helper->render($element);
        $this->assertNotContains('type="hidden"', $markup);
    }

    public function testRendersReCaptchaMarkup()
    {
        $element = $this->getElement();
        $markup  = $this->helper->render($element);
        $this->assertContains($this->captcha->getService()->getHtml($element->getName()), $markup);
    }
}

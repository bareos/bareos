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
use Zend\Form\Element\Csrf as CsrfElement;

class CsrfTest extends TestCase
{
    public function testProvidesInputSpecificationThatIncludesValidatorsBasedOnAttributes()
    {
        $element = new CsrfElement('foo');

        $inputSpec = $element->getInputSpecification();
        $this->assertArrayHasKey('validators', $inputSpec);
        $this->assertInternalType('array', $inputSpec['validators']);

        $expectedClasses = [
            'Zend\Validator\Csrf'
        ];
        foreach ($inputSpec['validators'] as $validator) {
            $class = get_class($validator);
            $this->assertContains($class, $expectedClasses, $class);
            switch ($class) {
                case 'Zend\Validator\Csrf':
                    $this->assertEquals('foo', $validator->getName());
                    break;
                default:
                    break;
            }
        }
    }

    public function testAllowSettingCustomCsrfValidator()
    {
        $element = new CsrfElement('foo');
        $validatorMock = $this->createMock('Zend\Validator\Csrf');
        $element->setCsrfValidator($validatorMock);
        $this->assertEquals($validatorMock, $element->getCsrfValidator());
    }

    public function testAllowSettingCsrfValidatorOptions()
    {
        $element = new CsrfElement('foo');
        $element->setCsrfValidatorOptions(['timeout' => 777]);
        $validator = $element->getCsrfValidator();
        $this->assertEquals('foo', $validator->getName());
        $this->assertEquals(777, $validator->getTimeout());
    }

    public function testAllowSettingCsrfOptions()
    {
        $element = new CsrfElement('foo');
        $element->setOptions([
            'csrf_options' => [
                'timeout' => 777,
                'salt' => 'MySalt']
            ]);
        $validator = $element->getCsrfValidator();
        $this->assertEquals('foo', $validator->getName());
        $this->assertEquals(777, $validator->getTimeOut());
        $this->assertEquals('MySalt', $validator->getSalt());
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Form;
use Zend\Form\Element;
use Zend\Validator;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;

class CustomForm extends Form
{
    public function __construct()
    {
        parent::__construct('test_form');

        $this
            ->setAttribute('method', 'post')
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator()
                : new ClassMethods()
            );

        $field1 = new Element('name', ['label' => 'Name']);
        $field1->setAttribute('type', 'text');
        $this->add($field1);

        $field2 = new Element('email', ['label' => 'Email']);
        $field2->setAttribute('type', 'text');
        $this->add($field2);

        $this->add([
            'name' => 'csrf',
            'type' => 'Zend\Form\Element\Csrf',
            'attributes' => [
            ],
        ]);

        $this->add([
            'name' => 'submit',
            'attributes' => [
                'type' => 'submit'
            ]
        ]);
    }

    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required' => true,
                'filters'  => [
                    ['name' => 'Zend\Filter\StringTrim'],
                ],
            ],
            'email' => [
                'required' => true,
                'filters'  => [
                    ['name' => 'Zend\Filter\StringTrim'],
                ],
                'validators' => [
                    new Validator\EmailAddress(),
                ],
            ],
        ];
    }
}

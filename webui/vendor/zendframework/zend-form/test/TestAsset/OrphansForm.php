<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Form;

class OrphansForm extends Form
{
    public function __construct()
    {
        parent::__construct('orphans');

        $this->setAttribute('method', 'post')
            ->setBindOnValidate(self::BIND_ON_VALIDATE)
            ->setInputFilter(new InputFilter());

        //adds a collection of 2
        $this->add(
            [
                'type' => '\Zend\Form\Element\Collection',
                'name' => 'test',
                'options' => [
                    'use_as_base_fieldset' => true,
                    'count' => 2,
                    'should_create_template' => true,
                    'allow_add' => true,
                    'target_element' => [
                        'type' => '\ZendTest\Form\TestAsset\OrphansFieldset'
                    ],
                ]
            ]
        );

        $this->add(
            [
                'name' => 'submit',
                'attributes' => [
                    'type' => 'submit',
                    'value' => 'Send'
                ]
            ]
        );

        $this->setValidationGroup(
            [
                'test' => [
                    'name',
                ],
            ]
        );
    }
}

<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Fieldset;
use Zend\Form\Element;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;
use Zend\InputFilter\InputFilterProviderInterface;
use ZendTest\Form\TestAsset\Entity\Phone;

class PhoneFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('phones');

        $this
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator()
                : new ClassMethods()
            )
             ->setObject(new Phone());

        $id = new Element\Hidden('id');
        $this->add($id);

        $number = new Element\Text('number');
        $number->setLabel('Number')
               ->setAttribute('class', 'form-control');
        $this->add($number);
    }

    public function getInputFilterSpecification()
    {
        return [
            'number' => [
                'required' => true,
            ]
        ];
    }
}

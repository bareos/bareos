<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Form;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;

class CreateAddressForm extends Form
{
    public function __construct()
    {
        parent::__construct('create_address');

        $this
            ->setAttribute('method', 'post')
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator(false)
                : new ClassMethods(false)
            )
            ->setInputFilter(new InputFilter());

        $address = new AddressFieldset();
        $address->setUseAsBaseFieldset(true);
        $this->add($address);

        $this->add([
            'name' => 'submit',
            'attributes' => [
                'type' => 'submit',
            ],
        ]);
    }
}

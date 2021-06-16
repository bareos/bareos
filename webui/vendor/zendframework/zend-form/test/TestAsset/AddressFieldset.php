<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;

class AddressFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('address');
        $this
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator(false)
                : new ClassMethods(false)
            )
            ->setObject(new Entity\Address());

        $street = new \Zend\Form\Element('street', ['label' => 'Street']);
        $street->setAttribute('type', 'text');

        $city = new CityFieldset;
        $city->setLabel('City');

        $this->add($street);
        $this->add($city);

        $phones = new \Zend\Form\Element\Collection('phones');
        $phones->setLabel('Phone numbers')
               ->setOptions([
                    'count'          => 2,
                    'allow_add'      => true,
                    'allow_remove'   => true,
                    'target_element' => new PhoneFieldset(),
               ]);
        $this->add($phones);
    }

    /**
     * Should return an array specification compatible with
     * {@link Zend\InputFilter\Factory::createInputFilter()}.
     *
     * @return array
     */
    public function getInputFilterSpecification()
    {
        return [
            'street' => [
                'required' => true,
            ]
        ];
    }
}

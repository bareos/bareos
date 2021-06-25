<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use ZendTest\Form\TestAsset\Entity\Product;
use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ClassMethods;
use Zend\Hydrator\ClassMethodsHydrator;

class ProductFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('product');
        $this
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator()
                : new ClassMethods()
            )
            ->setObject(new Product());

        $this->add([
            'name' => 'name',
            'options' => [
                'label' => 'Name of the product'
            ],
            'attributes' => [
                'required' => 'required'
            ]
        ]);

        $this->add([
            'name' => 'price',
            'options' => [
                'label' => 'Price of the product'
            ],
            'attributes' => [
                'required' => 'required'
            ]
        ]);

        $this->add([
            'type' => 'Zend\Form\Element\Collection',
            'name' => 'categories',
            'options' => [
                'label' => 'Please choose categories for this product',
                'count' => 2,
                'target_element' => [
                    'type' => 'ZendTest\Form\TestAsset\CategoryFieldset'
                ]
            ]
        ]);

        $this->add([
            'type' => 'ZendTest\Form\TestAsset\CountryFieldset',
            'name' => 'made_in_country',
            'object' => 'ZendTest\Form\TestAsset\Entity\Country',
            'hydrator' => class_exists(ClassMethodsHydrator::class)
                ? ClassMethodsHydrator::class
                : ClassMethods::class,
            'options' => [
                'label' => 'Please choose the country',
            ]
        ]);
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
            'name' => [
                'required' => true,
            ],
            'price' => [
                'required' => true,
                'validators' => [
                    [
                        'name' => 'IsFloat'
                    ]
                ]
            ],
            'made_in_country' => [
                'required' => false,
            ],
        ];
    }
}

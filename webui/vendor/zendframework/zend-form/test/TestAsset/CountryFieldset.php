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

class CountryFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('country');
        $this
            ->setHydrator(
                class_exists(ClassMethodsHydrator::class)
                ? new ClassMethodsHydrator()
                : new ClassMethods()
            )
            ->setObject(new Entity\Country());

        $name = new \Zend\Form\Element('name', ['label' => 'Name of the country']);
        $name->setAttribute('type', 'text');

        $continent = new \Zend\Form\Element('continent', ['label' => 'Continent of the city']);
        $continent->setAttribute('type', 'text');

        $this->add($name);
        $this->add($continent);
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
            'continent' => [
                'required' => true
            ]
        ];
    }
}

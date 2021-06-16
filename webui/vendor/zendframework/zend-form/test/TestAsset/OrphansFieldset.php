<?php
/**
 * @see       https://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-form/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Fieldset;
use Zend\InputFilter\InputFilterProviderInterface;
use Zend\Hydrator\ArraySerializable;
use Zend\Hydrator\ArraySerializableHydrator;
use ZendTest\Form\TestAsset\Entity\Orphan;

class OrphansFieldset extends Fieldset implements InputFilterProviderInterface
{
    public function __construct($name = null, $options = [])
    {
        parent::__construct($name, $options);

        $this
            ->setHydrator(
                class_exists(ArraySerializableHydrator::class)
                ? new ArraySerializableHydrator()
                : new ArraySerializable()
            )
            ->setObject(new Orphan());

        $this->add([
            'name'    => 'name',
            'options' => ['label' => 'Name field'],
        ]);
    }

    public function getInputFilterSpecification()
    {
        return [
            'name' => [
                'required'   => false,
                'filters'    => [],
                'validators' => [],
            ],
        ];
    }
}

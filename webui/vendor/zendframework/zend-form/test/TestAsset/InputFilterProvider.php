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
use Zend\InputFilter\InputFilterProviderInterface;

class InputFilterProvider extends Form implements InputFilterProviderInterface
{
    public function __construct($name = null, $options = [])
    {
        parent::__construct($name, $options);

        $this->add([
            'name' => 'foo',
            'options' => [
                'label' => 'Foo'
            ],
        ]);
    }

    public function getInputFilterSpecification()
    {
        return [
            'foo' => [
                'required' => true,
            ]
        ];
    }
}

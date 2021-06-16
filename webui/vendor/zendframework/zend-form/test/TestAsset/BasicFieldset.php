<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Element;
use Zend\Form\Fieldset;

class BasicFieldset extends Fieldset implements \Zend\InputFilter\InputFilterProviderInterface
{
    public function __construct()
    {
        parent::__construct('basic_fieldset');

        $field = new Element('field', ['label' => 'Name']);
        $field->setAttribute('type', 'text');
        $this->add($field);

        $nestedFieldset = new NestedFieldset();
        $this->add($nestedFieldset);
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
            'field' => [
                'required' => true
            ]
        ];
    }
}

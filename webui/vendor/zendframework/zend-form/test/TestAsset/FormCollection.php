<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Element\Color as ColorElement;
use Zend\Form\Form;

class FormCollection extends Form
{
    public function __construct()
    {
        parent::__construct('collection');
        $this->setInputFilter(new InputFilter());

        $element = new ColorElement('color');
        $this->add([
            'type' => 'Zend\Form\Element\Collection',
            'name' => 'colors',
            'options' => [
                'count' => 2,
                'target_element' => $element
            ]
        ]);

        $fieldset = new BasicFieldset();
        $this->add([
            'type' => 'Zend\Form\Element\Collection',
            'name' => 'fieldsets',
            'options' => [
                'count' => 2,
                'target_element' => $fieldset
            ]
        ]);
    }
}

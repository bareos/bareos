<?php
/**
 * @link      http://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\TestAsset;

use Zend\Form\Element;
use Zend\Form\Fieldset;

class FieldsetWithDependency extends Fieldset
{
    /**
     * @var InputFilter
     */
    private $dependency;

    public function __construct($name = null, $options = [])
    {
        parent::__construct('fieldset_with_dependency', $options);
    }

    public function init()
    {
        // should not fail
        $this->dependency->getValues();
    }

    /**
     * @return InputFilter
     */
    public function getDependency()
    {
        return $this->dependency;
    }

    /**
     * @param InputFilter $dependency
     */
    public function setDependency($dependency)
    {
        $this->dependency = $dependency;
    }
}

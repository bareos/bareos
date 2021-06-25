<?php
/**
 * @link      http://github.com/zendframework/zend-form for the canonical source repository
 * @copyright Copyright (c) 2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Integration\TestAsset;

use Zend\Form\Form as BaseForm;

class Form extends BaseForm
{
    /**
     * @param null|\Zend\Form\FormElementManager
     */
    public $elementManagerAtInit;

    /**
     * {@inheritDoc}
     */
    public function init()
    {
        $this->elementManagerAtInit = $this->getFormFactory()->getFormElementManager();
    }
}

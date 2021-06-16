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
use DateTime;

class CustomCreatedForm extends Form
{
    private $created;

    public function __construct(DateTime $created, $name = null, $options = [])
    {
        parent::__construct($name, $options);
        $this->created = $created;
    }

    public function getCreated()
    {
        return $this->created;
    }
}

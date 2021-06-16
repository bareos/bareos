<?php

namespace ZendTest\View\Helper\TestAsset;

use Zend\View\Helper\Placeholder\Container\AbstractContainer;

class MockContainer extends AbstractContainer
{
    public $data = [];

    public function __construct($data)
    {
        $this->data = $data;
    }
}

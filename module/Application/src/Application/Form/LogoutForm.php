<?php

namespace Application\Form;

use Zend\Form\Element\Submit;
use Zend\Form\Form;

class LogoutForm extends Form
{
    public function __construct()
    {
    
        parent::__construct('logout');
        
        $submitElement = new Submit('logout');
        $submitElement->setValue('Logout');
        $submitElement->setAttribute('class', 'btn');
        
        $this->add($submitElement);
        
    }
}

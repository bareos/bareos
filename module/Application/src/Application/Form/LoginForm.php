<?php
 
namespace Application\Form;
 
use Zend\Form\Element\Password;
use Zend\Form\Element\Submit;
use Zend\Form\Element\Text;
use Zend\Form\Form;
 
class LoginForm extends Form
{
 
      public function __construct()
      {
      
	    $usernameElement = new Text('username');
	    $usernameElement->setLabel('Username');
	    $passwordElement = new Password('password');
	    $passwordElement->setLabel('Password');
	    $submitElement = new Submit('login');
	    $submitElement->setValue('Login');
	    $submitElement->setAttribute('class', 'btn');
	    
	    $this->add($usernameElement);
	    $this->add($passwordElement);
	    $this->add($submitElement);
	    
      }
 
}
 
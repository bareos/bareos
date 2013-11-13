<?php

namespace Application\Form;

use Zend\Form\Form;
use Zend\Form\Element;
use Zend\InputFilter\Factory as InputFactory;
use Zend\InputFilter\InputFilter;

class Login extends Form
{

    public function __construct($name = null) {
    
	parent::__construct('login');
	$this->setAttribute('method', 'post');
	
	$username = new Element\Text('username');
	$username->setLabel('Username');
	$username->setAttribute('size', '32');
	$this->add($username);
	
	$password = new Element\Password('password');
	$password->setLabel('Password');
	$password->setAttribute('size', '10');
	$this->add($password);
	
	$csrf = new Element\Csrf('csrf');
	$this->add($csrf);
	
	$submit = new Element\Submit('submit');
	$submit->setValue('Log in');
	$this->add($submit);
	
	$inputFilter = new InputFilter();
	$factory = new InputFactory();
	
	$inputFilter->add($factory->createInput(array(
	    'name' => 'username',
	    'requiered' => true,
	    'filters' => array(array('name' => 'StringTrim'),),
	    'validators' => array(
		array(
		    'name' => 'StringLength', 
		    'options' = array('min' => 8)
		)
	    )
	)));
	
	$inputFilter->add($factory->createInput(array(
	    'name' => 'password',
	    'requiered' => true,
	    'validators' => array(
		array(
		    'name' => 'StringLength',
		    'options' => array(
			'min' => 5
		    )
		)
	    )
	)));

	$inputFilter->add($factory->createInput(array(
	    'name' => 'csrf',
	    'required' => true,
	    'validators' => array(
		array(
		    'name' => 'Csrf',
		    'options' => array(
			'timeout' => 600
		    )
		)
	    )
	)));
	
	$this->setInputFilter($inputFilter);
	
    }

}

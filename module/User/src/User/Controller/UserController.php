<?php

namespace User\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class UserController extends AbstractActionController 
{
	
	public function indexAction()
	{
		return new ViewModel();
	}

}


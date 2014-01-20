<?php

namespace Restore\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class RestoreController extends AbstractActionController 
{
	
	public function indexAction()
	{
		return new ViewModel();
	}

}


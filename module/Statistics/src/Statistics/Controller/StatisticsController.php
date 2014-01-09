<?php

namespace Statistics\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class StatisticsController extends AbstractActionController 
{
	
	public function indexAction()
	{
		return new ViewModel();
	}

}


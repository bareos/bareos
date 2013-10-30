<?php

namespace Pool\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class PoolController extends AbstractActionController
{

	protected $poolTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
				'pools' => $this->getPoolTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{

	}

	public function getPoolTable() 
	{
		if(!$this->poolTable) {
			$sm = $this->getServiceLocator();
			$this->poolTable = $sm->get('Pool\Model\PoolTable');
		}
		return $this->poolTable;
	}
	
}


<?php

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class ClientController extends AbstractActionController
{

	protected $clientTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
				'clients' => $this->getClientTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{

	}

	public function getClientTable() 
	{
		if(!$this->clientTable) {
			$sm = $this->getServiceLocator();
			$this->clientTable = $sm->get('Client\Model\ClientTable');
		}
		return $this->clientTable;
	}
	
}


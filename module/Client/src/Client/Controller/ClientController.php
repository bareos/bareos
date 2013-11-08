<?php

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class ClientController extends AbstractActionController
{

	protected $clientTable;
	protected $jobTable;

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
		
		$id = (int) $this->params()->fromRoute('id', 0);
		if(!$id) {
		    return $this->redirect()->toRoute('client');
		}
		
		return new ViewModel(
		    array(
		      'client' => $this->getClientTable()->getClient($id),
		      'job' => $this->getJobTable()->getLastSuccessfulClientJob($id),
		    )
		);
		
	}

	public function getClientTable() 
	{
		if(!$this->clientTable) {
			$sm = $this->getServiceLocator();
			$this->clientTable = $sm->get('Client\Model\ClientTable');
		}
		return $this->clientTable;
	}
	
	public function getJobTable() 
	{
		if(!$this->jobTable) {
			$sm = $this->getServiceLocator();
			$this->jobTable = $sm->get('Job\Model\JobTable');
		}
		return $this->jobTable;
	}
	
}


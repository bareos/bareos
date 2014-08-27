<?php

namespace Client\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Bareos\BConsole\BConsoleConnector;

class ClientController extends AbstractActionController
{

	protected $clientTable;
	protected $jobTable;

	public function indexAction()
	{
		$paginator = $this->getClientTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(25);

		return new ViewModel(array('paginator' => $paginator));
	}

	public function detailsAction() 
	{
		
		$id = (int) $this->params()->fromRoute('id', 0);
		if(!$id) {
		    return $this->redirect()->toRoute('client');
		}
		
		$result = $this->getClientTable()->getClient($id);
		$cmd = 'status client="' . $result->name . '"';
		$config = $this->getServiceLocator()->get('Config');
                $bcon = new BConsoleConnector($config['bconsole']);

		return new ViewModel(
		    array(
		      'client' => $this->getClientTable()->getClient($id),
		      'job' => $this->getJobTable()->getLastSuccessfulClientJob($id),
		      'bconsoleOutput' => $bcon->getBConsoleOutput($cmd),
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


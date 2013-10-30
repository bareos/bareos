<?php

namespace Log\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class LogController extends AbstractActionController
{

	protected $logTable;

	public function indexAction() 
	{

		$paginator = $this->getLogTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(5);

		return new ViewModel(array('paginator' => $paginator));

	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		if (!$id) {
		    return $this->redirect()->toRoute('log');
		}
	  
		return new ViewModel(array(
				'log' => $this->getLogTable()->getLog($id),
			));
	}

	public function getLogTable()
       	{
		if(!$this->logTable) {
			$sm = $this->getServiceLocator();
			$this->logTable = $sm->get('Log\Model\LogTable');
		}
		return $this->logTable;
	}

}


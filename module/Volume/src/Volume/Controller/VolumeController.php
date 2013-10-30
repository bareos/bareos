<?php

namespace Volume\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class VolumeController extends AbstractActionController
{

	protected $volumeTable;

	public function indexAction()
	{
		$paginator = $this->getVolumeTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(20);

		return new ViewModel(array('paginator' => $paginator));
	}

	public function detailsAction() 
	{
		
	}

	public function getVolumeTable() 
	{
		if(!$this->volumeTable) {
			$sm = $this->getServiceLocator();
			$this->volumeTable = $sm->get('Volume\Model\VolumeTable');
		}
		return $this->volumeTable;
	}
	
}


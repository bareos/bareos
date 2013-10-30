<?php

namespace Storage\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class StorageController extends AbstractActionController
{

	protected $storageTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
				'storages' => $this->getStorageTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{

	}

	public function getStorageTable() 
	{
		if(!$this->storageTable) {
			$sm = $this->getServiceLocator();
			$this->storageTable = $sm->get('Storage\Model\StorageTable');
		}
		return $this->storageTable;
	}
	
}


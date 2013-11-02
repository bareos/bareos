<?php

namespace Fileset\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class FilesetController extends AbstractActionController
{

	protected $filesetTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
				'filesets' => $this->getFilesetTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{

	}

	public function getFilesetTable() 
	{
		if(!$this->filesetTable) {
			$sm = $this->getServiceLocator();
			$this->filesetTable = $sm->get('Fileset\Model\FilesetTable');
		}
		return $this->filesetTable;
	}
	
}


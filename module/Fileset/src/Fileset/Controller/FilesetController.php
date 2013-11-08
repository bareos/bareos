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
		$id = (int) $this->params()->fromRoute('id', 0);
		
		if (!$id) {
		    return $this->redirect()->toRoute('fileset');
		}
	
		return new ViewModel(
			array(
				'fileset' => $this->getFilesetTable()->getFileset($id),
			)
		);
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


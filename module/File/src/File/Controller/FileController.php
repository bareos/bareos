<?php

namespace File\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class FileController extends AbstractActionController
{

	protected $fileTable;

	public function indexAction()
	{
		$paginator = $this->getFileTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(15);

		return new ViewModel( array('paginator' => $paginator) );
	}

	public function revisionsAction() 
	{

	}

	public function jobidAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		
		if (!$id) {
		    return $this->redirect()->toRoute('job');
		}
	
		$paginator = $this->getFileTable()->getFileByJobId($id);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(20);

		return new ViewModel( array('paginator' => $paginator) );
	}
	
	public function getFileTable() 
	{
		if(!$this->fileTable) {
			$sm = $this->getServiceLocator();
			$this->fileTable = $sm->get('File\Model\FileTable');
		}
		return $this->fileTable;
	}
	
}


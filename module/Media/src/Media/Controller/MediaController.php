<?php

namespace Media\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class MediaController extends AbstractActionController
{

	protected $mediaTable;

	public function indexAction()
	{
		$paginator = $this->getMediaTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(array('paginator' => $paginator));
	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		if (!$id) {
		    return $this->redirect()->toRoute('media');
		}
	  
		return new ViewModel(array(
				'media' => $this->getMediaTable()->getMedia($id),
			));
	}

	public function getMediaTable() 
	{
		if(!$this->mediaTable) {
			$sm = $this->getServiceLocator();
			$this->mediaTable = $sm->get('Media\Model\MediaTable');
		}
		return $this->mediaTable;
	}
	
}


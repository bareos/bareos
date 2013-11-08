<?php

namespace Pool\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class PoolController extends AbstractActionController
{

	protected $poolTable;
	protected $mediaTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
				'pools' => $this->getPoolTable()->fetchAll(),
			)
		);
	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		
		if (!$id) {
		    return $this->redirect()->toRoute('pool');
		}
		
		return new ViewModel(
			array(
				'pool' => $this->getPoolTable()->getPool($id),
				'media' => $this->getMediaTable()->getPoolVolumes($id),
			)
		);
		
	}

	public function getPoolTable() 
	{
		if(!$this->poolTable) {
			$sm = $this->getServiceLocator();
			$this->poolTable = $sm->get('Pool\Model\PoolTable');
		}
		return $this->poolTable;
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


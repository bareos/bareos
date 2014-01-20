<?php

namespace Job\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class JobController extends AbstractActionController
{

	protected $jobTable;

	public function indexAction() 
	{

		$paginator = $this->getJobTable()->fetchAll(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(array('paginator' => $paginator));

	}

	public function detailsAction() 
	{
		$id = (int) $this->params()->fromRoute('id', 0);
		if (!$id) {
		    return $this->redirect()->toRoute('job');
		}
	  
		return new ViewModel(array(
				'job' => $this->getJobTable()->getJob($id),
			));
	}

	public function runningAction() 
	{
		$paginator = $this->getJobTable()->getRunningJobs(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(
			array(
			      'paginator' => $paginator,
			      'runningJobs' => $this->getJobTable()->getRunningJobs()
			    )
			);
	}
	
	public function waitingAction() 
	{
		$paginator = $this->getJobTable()->getWaitingJobs(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(
			array(
			      'paginator' => $paginator,
			      'waitingJobs' => $this->getJobTable()->getWaitingJobs()
			    )
			);
	}
	
	public function problemAction() 
	{
		$paginator = $this->getJobTable()->getLast24HoursUnsuccessfulJobs(true);
		$paginator->setCurrentPageNumber( (int) $this->params()->fromQuery('page', 1) );
		$paginator->setItemCountPerPage(10);

		return new ViewModel(
			array(
			      'paginator' => $paginator,
			      'lastUnsuccessfulJobs' => $this->getJobTable()->getLast24HoursUnsuccessfulJobs(),
			    )
			);
	}
	
	public function timelineAction() 
	{
		return new ViewModel();
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


<?php

namespace Dashboard\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DashboardController extends AbstractActionController
{

	protected $jobTable;
	
	public function indexAction()
	{
		return new ViewModel(
			array(
			
				'lastSuccessfulJobs' => $this->getJobTable()->getLast24HoursSuccessfulJobs(),
				'lastUnsuccessfulJobs' => $this->getJobTable()->getLast24HoursUnsuccessfulJobs(),
				'waitingJobs' => $this->getJobTable()->getWaitingJobs(),
				'runningJobs' => $this->getJobTable()->getRunningJobs(),
			
			)
		);
	}

	public function getJobTable()
	{
		if(!$this->jobTable)
		{
			$sm = $this->getServiceLocator();
			$this->jobTable = $sm->get('Job\Model\JobTable');
		}
		return $this->jobTable;
	}
	
}


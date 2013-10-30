<?php

namespace Dashboard\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class DashboardController extends AbstractActionController
{

	protected $jobTable;
	protected $clientTable;
	protected $poolTable;
	protected $volumeTable;
	protected $fileTable;

	public function indexAction()
	{
		return new ViewModel(
			array(
			
				'jobsCreatedNotRunning' => $this->getJobTable()->getJobCountLast24HoursByStatus("C"),
				'jobsBlocked' => $this->getJobTable()->getJobCountLast24HoursByStatus("B"),
				'jobsRunning' => $this->getJobTable()->getJobCountLast24HoursByStatus("R"),
				'jobsTerminated' => $this->getJobTable()->getJobCountLast24HoursByStatus("T"),
				'jobsTerminatedWithErrors' => $this->getJobTable()->getJobCountLast24HoursByStatus("E"),
				'jobsWithNonFatalErrors' => $this->getJobTable()->getJobCountLast24HoursByStatus("e"),
				'jobsWithFatalErrors' => $this->getJobTable()->getJobCountLast24HoursByStatus("f"),
				'jobsCanceled' => $this->getJobTable()->getJobCountLast24HoursByStatus("A"),
				'jobsVerifyFoundDiff' => $this->getJobTable()->getJobCountLast24HoursByStatus("D"),
				'jobsWaitingForClient' => $this->getJobTable()->getJobCountLast24HoursByStatus("F"),
				'jobsWaitingForStorageDaemon' => $this->getJobTable()->getJobCountLast24HoursByStatus("S"),
				'jobsWaitingForNewMedia' => $this->getJobTable()->getJobCountLast24HoursByStatus("m"),
				'jobsWaitingForMediaMount' => $this->getJobTable()->getJobCountLast24HoursByStatus("M"),
				'jobsWaitingForStorageResource' => $this->getJobTable()->getJobCountLast24HoursByStatus("s"),
				'jobsWaitingForJobResource' => $this->getJobTable()->getJobCountLast24HoursByStatus("j"),
				'jobsWaitingForClientResource' => $this->getJobTable()->getJobCountLast24HoursByStatus("c"),
				'jobsWaitingOnMaximumJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("d"),
				'jobsWaitingOnStartTime' => $this->getJobTable()->getJobCountLast24HoursByStatus("t"),
				'jobsWaitingOnHigherPriorityJobs' => $this->getJobTable()->getJobCountLast24HoursByStatus("p"),
				'jobsSDdespoolingAttributes' => $this->getJobTable()->getJobCountLast24HoursByStatus("a"),
				'jobsBatchInsertFileRecords' => $this->getJobTable()->getJobCountLast24HoursByStatus("i"),
				
				'clientNum' => $this->getClientTable()->getClientNum(),
				'poolNum' => $this->getPoolTable()->getPoolNum(),
				'volumeNum' => $this->getVolumeTable()->getVolumeNum(),
				'fileNum' => $this->getFileTable()->getFileNum(),
				
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
	
	public function getClientTable()
	{
		if(!$this->clientTable)
		{
			$sm = $this->getServiceLocator();
			$this->clientTable = $sm->get('Client\Model\ClientTable');
		}
		return $this->clientTable;
	}
	
	public function getPoolTable() 
	{
		if(!$this->poolTable)
		{
			$sm = $this->getServiceLocator();
			$this->poolTable = $sm->get('Pool\Model\PoolTable');
		}
		return $this->poolTable;
	}
	
	public function getVolumeTable() 
	{
		if(!$this->volumeTable)
		{
			$sm = $this->getServiceLocator();
			$this->volumeTable = $sm->get('Volume\Model\VolumeTable');
		}
		return $this->volumeTable;
	}
	
	public function getFileTable() 
	{
		if(!$this->fileTable)
		{
			$sm = $this->getServiceLocator();
			$this->fileTable = $sm->get('File\Model\FileTable');
		}
		return $this->fileTable;
	}
	
}


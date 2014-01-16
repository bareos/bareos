<?php

namespace Statistics\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;

class StatisticsController extends AbstractActionController 
{
	protected $jobTable;
	protected $clientTable;
	protected $poolTable;
	protected $mediaTable;
	protected $fileTable;
	protected $logTable;
	protected $filesetTable;
	
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
				
			)
		);
	}

	public function catalogAction()
	{
		return new ViewModel(
		      array(
			
				'clientNum' => $this->getClientTable()->getClientNum(),
				'poolNum' => $this->getPoolTable()->getPoolNum(),
				'volumeNum' => $this->getMediaTable()->getMediaNum(),
				'fileNum' => $this->getFileTable()->getFileNum(),
				'jobNum' => $this->getJobTable()->getJobNum(),
				'logNum' => $this->getLogTable()->getLogNum(),
				'filesetNum' => $this->getFilesetTable()->getFilesetNum(),
				
			)
		);
	}
	
	public function storedAction() 
	{
		return new ViewModel(
		      array(
		      )
		);
	}
	
	public function clientAction() 
	{
		return new ViewModel(
		);
	}
	
	public function backupjobAction() 
	{
		return new ViewModel(
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
	
	public function getMediaTable() 
	{
		if(!$this->mediaTable)
		{
			$sm = $this->getServiceLocator();
			$this->mediaTable = $sm->get('Media\Model\MediaTable');
		}
		return $this->mediaTable;
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
	
	public function getLogTable() 
	{
		if(!$this->logTable)
		{
			$sm = $this->getServiceLocator();
			$this->logTable = $sm->get('Log\Model\LogTable');
		}
		return $this->logTable;
	}
	
	public function getFilesetTable() 
	{
		if(!$this->filesetTable)
		{
			$sm = $this->getServiceLocator();
			$this->filesetTable = $sm->get('Fileset\Model\FilesetTable');
		}
		return $this->filesetTable;
	}
	
}


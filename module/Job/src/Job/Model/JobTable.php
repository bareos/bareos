<?php

namespace Job\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;

class JobTable
{
	protected $tableGateway;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll($paginated=false)
	{
		if($paginated) {
			$select = new Select();
			$select->from('job');
			$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
			$select->order('job.jobid DESC');
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Job());
			$paginatorAdapter = new DbSelect(
						$select,
						$this->tableGateway->getAdapter(),
						$resultSetPrototype
					);
			$paginator = new Paginator($paginatorAdapter);
			return $paginator;
		}

		$resultSet = $this->tableGateway->select();
		return $resultSet;
	}

	public function getJob($jobid)
	{
		$jobid = (int) $jobid;
		
		$select = new Select();
		$select->from('job');
		$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
		$select->where('jobid =' . $jobid);
		
		$rowset = $this->tableGateway->selectWith($select);
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $jobid");
		}
		return $row;
	}

	public function getRunningJobs($paginated=false)
	{
		$select = new Select();
		$select->from('job');
		$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
		$select->order('job.jobid DESC');
		$select->where("jobstatus = 'R'");
		
		if($paginated) {
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Job());
			$paginatorAdapter = new DbSelect(
						$select,
						$this->tableGateway->getAdapter(),
						$resultSetPrototype
					);
			$paginator = new Paginator($paginatorAdapter);
			return $paginator;
		}
		else {
			$resultSet = $this->tableGateway->selectWith($select);      
			return $resultSet;
		}
	}
	
	public function getWaitingJobs() 
	{
		$select = new Select();
		$select->from('job');
		$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
		$select->order('job.jobid DESC');
		$select->where("jobstatus = 'F' OR 
				jobstatus = 'S' OR 
				jobstatus = 'm' OR 
				jobstatus = 'M' OR 
				jobstatus = 's' OR 
				jobstatus = 'j' OR 
				jobstatus = 'c' OR 
				jobstatus = 'd' OR 
				jobstatus = 't' OR 
				jobstatus = 'p' OR
				jobstatus = 'C'");
		
		$resultSet = $this->tableGateway->selectWith($select);
		
		return $resultSet;
	}
	
	public function getLast24HoursSuccessfulJobs()
	{
		$current_time = date("Y-m-d H:i:s",time());
		$back24h_time = date("Y-m-d H:i:s",time() - (60*60*23));
		
		$select = new Select();
		$select->from('job');
		$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
		$select->order('job.jobid DESC');
		$select->where("jobstatus = 'T' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
				
		$resultSet = $this->tableGateway->selectWith($select);
		
		return $resultSet;
	}
	
	public function getLast24HoursUnsuccessfulJobs($paginated=false)
	{
		$current_time = date("Y-m-d H:i:s",time());
		$back24h_time = date("Y-m-d H:i:s",time() - (60*60*23));
		
		$select = new Select();
		$select->from('job');
		$select->join('client', 'job.clientid = client.clientid', array('clientname' => 'name'));
		$select->order('job.jobid DESC');
		$select->where("jobstatus != 'T' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
				
		if($paginated) {
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Job());
			$paginatorAdapter = new DbSelect(
						$select,
						$this->tableGateway->getAdapter(),
						$resultSetPrototype
					);
			$paginator = new Paginator($paginatorAdapter);
			return $paginator;
		}
		else {
			$resultSet = $this->tableGateway->selectWith($select);      
			return $resultSet;
		}
	}
	
	public function getJobCountLast24HoursByStatus($status) 
	{
		$current_time = date("Y-m-d H:i:s",time());
		$back24h_time = date("Y-m-d H:i:s",time() - (60*60*23));
	
		$select = new Select();
		$select->from('job');
		
		if($status == "C")
		{
			$select->where("jobstatus = 'C' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "B")
		{
			$select->where("jobstatus = 'B' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "T")
		{
			$select->where("jobstatus = 'T' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "R")
		{
			$select->where("jobstatus = 'R' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "E")
		{
			$select->where("jobstatus = 'E' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "e")
		{
			$select->where("jobstatus = 'e' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "f")
		{
			$select->where("jobstatus = 'f' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "A")
		{
			$select->where("jobstatus = 'A' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "D")
		{
			$select->where("jobstatus = 'D' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "F")
		{
			$select->where("jobstatus = 'F' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "S")
		{
			$select->where("jobstatus = 'S' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "m")
		{
			$select->where("jobstatus = 'm' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "M")
		{
			$select->where("jobstatus = 'M' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "s")
		{
			$select->where("jobstatus = 's' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "j")
		{
			$select->where("jobstatus = 'j' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "c")
		{
			$select->where("jobstatus = 'c' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "d")
		{
			$select->where("jobstatus = 'd' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "t")
		{
			$select->where("jobstatus = 't' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "p")
		{
			$select->where("jobstatus = 'p' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "a")
		{
			$select->where("jobstatus = 'a' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
		if($status == "i")
		{
			$select->where("jobstatus = 'i' AND starttime >= '" . $back24h_time . "' AND endtime >= '" . $back24h_time . "'");
		}
				
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Job());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		
		return $num;
	}
	
	public function getJobNum()
	{
		$select = new Select();
		$select->from('job');
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Job());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
	public function getLastSuccessfulClientJob($id) 
	{
		$select = new Select();
		$select->from('job');
		$select->where("clientid = " . $id . " AND jobstatus = 'T'");
		$select->order('jobid DESC');
		$select->limit(1);
		
		$rowset = $this->tableGateway->selectWith($select);
		$row = $rowset->current();
		
		if(!$row) {
			throw new \Exception("Could not find row $jobid");
		}
		
		return $row;
	}
	
}

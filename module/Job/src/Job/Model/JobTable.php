<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 * 
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 dass-IT GmbH (http://www.dass-it.de/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Job\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Db\Sql\Expression;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;
use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Bareos\Db\Sql\BareosSqlCompatHelper;

class JobTable implements ServiceLocatorAwareInterface
{
	protected $tableGateway;
	protected $serviceLocator;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function setServiceLocator(ServiceLocatorInterface $serviceLocator) {
    		$this->serviceLocator = $serviceLocator;
  	}

  	public function getServiceLocator() {
    		return $this->serviceLocator;
  	}

	public function getDbDriverConfig() {
		$config = $this->getServiceLocator()->get('Config');
		return $config['db']['driver'];
	}

	public function fetchAll($paginated=false, $order_by=null, $order=null)
	{
		if($this->getDbDriverConfig() == "Pdo_Mysql" || $this->getDbDriverConfig() == "Mysqli") {
			$duration = new Expression("TIMESTAMPDIFF(SECOND, StartTime, EndTime)");
		}
		elseif($this->getDbDriverConfig() == "Pdo_Pgsql" || $this->getDbDriverConfig() == "Pgsql") {
			$duration = new Expression("DATE_PART('second', 'EndTime'::timestamp - 'StartTime'::timestamp)");
		}

		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->columns(array(
				$bsqlch->strdbcompat("JobId"),
				$bsqlch->strdbcompat("Name"),
				$bsqlch->strdbcompat("Type"),
				$bsqlch->strdbcompat("Level"),
				$bsqlch->strdbcompat("StartTime"),
				$bsqlch->strdbcompat("EndTime"),
				$bsqlch->strdbcompat("JobStatus"),
				'duration' => $duration,
			)
		);
		$select->join(
			$bsqlch->strdbcompat("Client"), 
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"), 
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);

		if($order_by != null && $order != null) {
			$select->order($bsqlch->strdbcompat($order_by) . " " . $order);
		}
		else {
			$select->order($bsqlch->strdbcompat("JobId") . " DESC");
		}	

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

	public function getJob($jobid)
	{
		$jobid = (int) $jobid;
		
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->join(
			$bsqlch->strdbcompat("Client"),
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"),
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);
		$select->where(
			$bsqlch->strdbcompat("JobId") . "=" . $jobid
		);
		
		$rowset = $this->tableGateway->selectWith($select);
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $jobid");
		}
		return $row;
	}

	public function getRunningJobs($paginated=false, $order_by=null, $order=null)
	{
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->join(
			$bsqlch->strdbcompat("Client"),
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"), 
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);
		$select->where(
			$bsqlch->strdbcompat("JobStatus") . " = 'R' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'l'"
		);
		
		if($order_by != null && $order != null) {	
			$select->order($bsqlch->strdbcompat($order_by) . " " . $order);
		}
		else {
			$select->order($bsqlch->strdbcompat("JobId") . " DESC");
		}

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
	
	public function getWaitingJobs($paginated=false, $order_by=null, $order=null) 
	{
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->join(
			$bsqlch->strdbcompat("Client"), 
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"), 
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);
		$select->where(
			$bsqlch->strdbcompat("JobStatus") . " = 'F' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 'S' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'm' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'M' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 's' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 'j' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'c' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 'd' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 't' OR " . 
			$bsqlch->strdbcompat("JobStatus") . " = 'p' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'q' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'C'"
		);

		if($order_by != null && $order != null) {
			$select->order($bsqlch->strdbcompat($order_by) . " " . $order);
		}
		else {
			$select->order($bsqlch->strdbcompat("Job.JobId") . " DESC");
		}
		
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
	
	public function getLast24HoursSuccessfulJobs($paginated=false, $order_by=null, $order=null)
	{
		if($this->getDbDriverConfig() == "Pdo_Mysql" || $this->getDbDriverConfig() == "Mysqli") {       
                        $duration = new Expression("TIMESTAMPDIFF(SECOND, StartTime, EndTime)");                
			$interval = "now() - interval 1 day";
                }
                elseif($this->getDbDriverConfig() == "Pdo_Pgsql" || $this->getDbDriverConfig() == "Pgsql") {    
                        $duration = new Expression("DATE_PART('second', 'EndTime'::timestamp - 'StartTime'::timestamp)");
			$interval = "now() - interval '1 day'";
                }
	
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->columns(array(
                                $bsqlch->strdbcompat("JobId"),
                                $bsqlch->strdbcompat("Name"),
                                $bsqlch->strdbcompat("Type"),                                                   
                                $bsqlch->strdbcompat("Level"),
                                $bsqlch->strdbcompat("StartTime"),                                              
                                $bsqlch->strdbcompat("EndTime"),
                                $bsqlch->strdbcompat("JobStatus"),                                              
                                'duration' => $duration,
                        )
                );
		$select->join(
			$bsqlch->strdbcompat("Client"), 
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"), 
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);

		$select->where(
			"(" . 
			$bsqlch->strdbcompat("JobStatus") . " = 'T' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'W' ) AND (" .
			$bsqlch->strdbcompat("StartTime") . " >= " . $interval . " OR " .
			$bsqlch->strdbcompat("EndTime") . " >= " . $interval . ")"
		);

		if($order_by != null && $order != null) { 
			$select->order($bsqlch->strdbcompat($order_by) . " " . $order);
		}
		else {
			$select->order($bsqlch->strdbcompat("Job.JobId") . " DESC");
		}

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
	
	public function getLast24HoursUnsuccessfulJobs($paginated=false, $order_by=null, $order=null)
	{
		if($this->getDbDriverConfig() == "Pdo_Mysql" || $this->getDbDriverConfig() == "Mysqli") {
                        $duration = new Expression("TIMESTAMPDIFF(SECOND, StartTime, EndTime)");
                        $interval = "now() - interval 1 day";
                }
                elseif($this->getDbDriverConfig() == "Pdo_Pgsql" || $this->getDbDriverConfig() == "Pgsql") {
                        $duration = new Expression("DATE_PART('second', 'EndTime'::timestamp - 'StartTime'::timestamp)");
                        $interval = "now() - interval '1 day'";
                }

		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->columns(array(
                                $bsqlch->strdbcompat("JobId"),
                                $bsqlch->strdbcompat("Name"),
                                $bsqlch->strdbcompat("Type"),
                                $bsqlch->strdbcompat("Level"),
                                $bsqlch->strdbcompat("StartTime"),
                                $bsqlch->strdbcompat("EndTime"),
                                $bsqlch->strdbcompat("JobStatus"),
                                'duration' => $duration,
                        )
                );
		$select->join(
			$bsqlch->strdbcompat("Client"), 
			$bsqlch->strdbcompat("Job.ClientId = Client.ClientId"), 
			array($bsqlch->strdbcompat("ClientName") => $bsqlch->strdbcompat("Name"))
		);
		$select->where(
			"(" . 
			$bsqlch->strdbcompat("JobStatus") . " = 'A' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'E' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'e' OR " .
                        $bsqlch->strdbcompat("JobStatus") . " = 'f' ) AND (" .
                        $bsqlch->strdbcompat("StartTime") . " >= " . $interval . " OR " .
                        $bsqlch->strdbcompat("EndTime") . " >= " . $interval . ")"
		);		

		if($order_by != null && $order != null) {
			$select->order($bsqlch->strdbcompat($order_by) . " " . $order);
		}
		else {
			$select->order($bsqlch->strdbcompat("Job.JobId") . " DESC");
		}

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
	
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		
		if($status == "C")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'C' AND " .
				$bsqlch->strdbcompat("SchedTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "B")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'B' AND " . 
				$bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
				$bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "T")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'T' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "R")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'R' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "E")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'E' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "e")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'e' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "f")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'f' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "A")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'A' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "D")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'D' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "F")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'F' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "S")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'S' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "m")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'm' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "M")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'M' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "s")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 's' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "j")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'j' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "c")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'c' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "d")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'd' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "t")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 't' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "p")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'p' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "a")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'a' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
		}
		if($status == "i")
		{
			$select->where(
				$bsqlch->strdbcompat("JobStatus") . " = 'i' AND " . 
                                $bsqlch->strdbcompat("StartTime") . " >= '" . $back24h_time . "' AND " .
                                $bsqlch->strdbcompat("EndTime") . " >= '" . $back24h_time . "'"
			);
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
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
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
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->where(	
			$bsqlch->strdbcompat("ClientId") . " = " . $id . " AND (" .
			$bsqlch->strdbcompat("JobStatus") . " = 'T' OR " .
			$bsqlch->strdbcompat("JobStatus") . " = 'W')"
		);
		$select->order($bsqlch->strdbcompat("JobId") . " DESC");
		$select->limit(1);
		
		$rowset = $this->tableGateway->selectWith($select);
		$row = $rowset->current();
		
		if(!$row) {
			// Note: If there is no record, a job for this client was never executed.
			// Exception: throw new \Exception("Could not find row $jobid");
			$row = null;
		}
		
		return $row;
	}

	public function getStoredBytes7Days() 
	{
		$end = date("Y-m-d H:i:s",time());
		$start = date("Y-m-d H:i:s",time() - (60*60*23*7));
		
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		$select->columns(array(
					$bsqlch->strdbcompat("EndTime"),
					$bsqlch->strdbcompat("JobBytes"), 
					true
				)
		);
		$select->where(
				$bsqlch->strdbcompat("EndTime") . " >= '" . $start . "' AND " .
				$bsqlch->strdbcomapt("EndTime") . " <= '" . $end . "'"
		);
		$select->order($bsqlch->strdbcompat("EndTime" . " ASC"));
		
		$resultSet = $this->tableGateway->selectWith($select);
		return $resultSet;
	}
	
	public function getStoredBytes14Days() 
	{
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Job"));
		
		$resultSet = $this->tableGateway->selectWith($select);
		return $resultSet;
	}
	
}

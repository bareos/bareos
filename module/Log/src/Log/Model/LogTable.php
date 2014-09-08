<?php

namespace Log\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;
use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Bareos\Db\Sql\BareosSqlCompatHelper;

class LogTable implements ServiceLocatorAwareInterface
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
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
                $select = new Select($bsqlch->strdbcompat("Log"));

		if($order_by != null && $order != null) {
                        $select->order($bsqlch->strdbcompat($order_by) . " " . $order);
                }
		else {
                	$select->order($bsqlch->strdbcompat("LogId") . " DESC");
		}

		if($paginated) {
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Log());
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

	public function getLog($logid)
	{
		$logid = (int) $logid;
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());	
		$rowset = $this->tableGateway->select(array(
			$bsqlch->strdbcompat("LogId") => $logid)
		);
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $logid");
		}
		return $row;
	}

	public function getLogsByJob($id)
	{

		$jobid = (int) $id;
		
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Log"));
		$select->where($bsqlch->strdbcompat("JobId") . " = " . $jobid);
		$select->order($bsqlch->strdbcompat("LogId") . " DESC");

		$resultSet = $this->tableGateway->selectWith($select);

		return $resultSet;

	}
	
	public function getLogNum()
	{
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Log"));
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Log());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


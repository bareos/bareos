<?php

namespace Log\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;

class LogTable
{
	protected $tableGateway;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll($paginated=false)
	{
		if($paginated) {
			$select = new Select('log');
			$select->order('logid DESC');
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

		$resultSet = $this->tableGateway->select();
		return $resultSet;
	}

	public function getLog($logid)
	{
		$logid = (int) $logid;
		$rowset = $this->tableGateway->select(array('logid' => $logid));
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $logid");
		}
		return $row;
	}

	public function getLogsByJob($id)
	{
		
	}
	
	public function getLogNum()
	{
		$select = new Select();
		$select->from('log');
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

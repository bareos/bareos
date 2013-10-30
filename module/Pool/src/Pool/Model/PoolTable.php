<?php

namespace Pool\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;

class PoolTable
{

	protected $tableGateway;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll() 
	{
		$resultSet = $this->tableGateway->select();
		return $resultSet;
	}

	public function getPool($id)
	{
		$id = (int) $id;
		$rowset = $this->tableGateway->select(array('poolid' => $id));
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $id");
		}
		return $row;
	}

	public function getPoolNum()
	{
		$select = new Select();
		$select->from('pool');
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Pool());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


<?php

namespace Storage\Model;

use Zend\Db\TableGateway\TableGateway;

class StorageTable
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

	public function getStorage($id)
	{
		$id = (int) $id;
		$rowset = $this->tableGateway->select(array('id' => $id));
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $id");
		}
		return $row;
	}

}


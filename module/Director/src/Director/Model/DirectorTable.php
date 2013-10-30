<?php

namespace Director\Model;

//use Zend\Db\TableGateway\TableGateway;

class DirectorTable 
{
//	protected $tableGateway;
/*
	public function __construct(TableGateway $tableGateway) 
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll() 
	{
		$resultSet = $this->tableGateway->select();
		return $resultSet;
	}
*/
	
	public function __construct() 
	{
	}

	public function getStatus() 
	{
		$status = exec('ls -al');
		return $status;
	}
	
}


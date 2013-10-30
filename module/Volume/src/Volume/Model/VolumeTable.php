<?php

namespace Volume\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Db\Sql\Sql;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;

class VolumeTable
{

	protected $tableGateway;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll($paginated=false) 
	{
		if($paginated) {
			$select = new Select('media');
			$select->order('mediaid ASC');
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Volume());
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

	public function getVolume($id)
	{
				
	}

	public function getVolumeNum()
	{
		$select = new Select();
		$select->from('media');
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Volume());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


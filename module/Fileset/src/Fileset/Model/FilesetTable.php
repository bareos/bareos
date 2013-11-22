<?php

namespace Fileset\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;

class FilesetTable
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

	public function getFileset($id)
	{
		$id = (int) $id;
		$rowset = $this->tableGateway->select(array('filesetid' => $id));
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $id");
		}
		return $row;
	}

	public function getFilesetHistory($id)
	{
		$fset = $this->getFileSet($id);
                  
                $select = new Select();
                $select->from('fileset');
                $select->where("fileset = '". $fset->fileset ."'");
                $select->order('createtime DESC');
                
                $resultSet = $this->tableGateway->selectWith($select);
                  
                return $resultSet;
        }
	
	public function getFilesetNum()
	{
		$select = new Select();
		$select->from('fileset');
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Fileset());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


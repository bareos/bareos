<?php

namespace File\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;

class FileTable
{

	protected $tableGateway;

	public function __construct(TableGateway $tableGateway)
	{
		$this->tableGateway = $tableGateway;
	}

	public function fetchAll($paginated=false) 
	{
		if($paginated) {
			$select = new Select('file');
			$select->order('fileid DESC');
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new File());
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

	public function getFile($id)
	{
		$id = (int) $id;
		$rowset = $this->tableGateway->select(array('fileid' => $id));
		$row = $rowset->current();
		if(!$row) {
			throw new \Exception("Could not find row $id");
		}
		return $row;
	}
	
	public function getFileByJobId($id)
	{
		$paginated = true;
		$jobid = (int) $id;
		if($paginated) {
			$select = new Select();
			$select->from('file')->join('filename', 'file.filenameid = filename.filenameid');
			$select->where('file.jobid = ' . $jobid);
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new File());
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

	public function getFileNum()
	{
		$select = new Select();
		$select->from('file');
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new File());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


<?php

namespace Client\Model;

use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Zend\Db\Sql\Select;
use Zend\Paginator\Adapter\DbSelect;
use Zend\Paginator\Paginator;
use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Bareos\Db\Sql\BareosSqlCompatHelper;

class ClientTable implements ServiceLocatorAwareInterface
{

	protected $tableGateway;

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
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Client"));
	
		if($order_by != null && $order != null) {
                        $select->order($bsqlch->strdbcompat($order_by) . " " . $order);
                }
                else {
                        $select->order($bsqlch->strdbcompat("ClientId") . " DESC");
                }

		if($paginated) {
			$resultSetPrototype = new ResultSet();
			$resultSetPrototype->setArrayObjectPrototype(new Client());
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

	public function getClient($id)
	{
		$id = (int) $id;
		
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());	
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Client"));
		$select->where($bsqlch->strdbcompat("ClientId") . " = " . $id);
		
		$rowset = $this->tableGateway->selectWith($select);
		$row = $rowset->current();
		
		if(!$row) {
			throw new \Exception("Could not find row $id");
		}
		
		return $row;
	}

	public function getClientNum()
	{
		$bsqlch = new BareosSqlCompatHelper($this->getDbDriverConfig());
		$select = new Select();
		$select->from($bsqlch->strdbcompat("Client"));
		$resultSetPrototype = new ResultSet();
		$resultSetPrototype->setArrayObjectPrototype(new Client());
		$rowset = new DbSelect(
			$select,
			$this->tableGateway->getAdapter(),
			$resultSetPrototype
		);
		$num = $rowset->count();		  
		return $num;
	}
	
}


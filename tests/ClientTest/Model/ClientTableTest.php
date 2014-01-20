<?php

namespace ClientTest\Model;

use Client\Model\ClientTable;
use Client\Model\Client;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class ClientTableTest extends PHPUnit_Framework_TestCase 
{
	
	public function testFetchAllReturnsAllClients() 
	{
		$resultSet = new ResultSet();

		$mockTableGateway = $this->getMock(
			'Zend\Db\TableGateway\TableGateway',
			array('select'),
			array(),
			'',
			false
		);

		$mockTableGateway->expects($this->once())
			->method('select')
			->with()
			->will($this->returnValue($resultSet));

		$clientTable = new ClientTable($mockTableGateway);

		$this->assertSame($resultSet, $clientTable->fetchAll());

	}

}

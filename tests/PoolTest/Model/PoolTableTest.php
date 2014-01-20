<?php

namespace PoolTest\Model;

use Pool\Model\PoolTable;
use Pool\Model\Job;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class PoolTableTest extends PHPUnit_Framework_TestCase 
{
	public function testFetchAllReturnsAllPools() 
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

		$poolTable = new PoolTable($mockTableGateway);

		$this->assertSame($resultSet, $poolTable->fetchAll());

	}
}

<?php

namespace LogTest\Model;

use Log\Model\LogTable;
use Log\Model\Log;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class LogTableTest extends PHPUnit_Framework_TestCase
{
	public function testFetchAllReturnsAllLogs() 
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

		$logTable = new LogTable($mockTableGateway);

		$this->assertSame($resultSet, $logTable->fetchAll());

	}
}

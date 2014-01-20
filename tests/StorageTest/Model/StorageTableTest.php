<?php

namespace StorageTest\Model;

use Storage\Model\StorageTable;
use Storage\Model\Storage;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class StorageTableTest extends PHPUnit_Framework_TestCase
{

	public function testFetchAllReturnsAllJobs()
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

		$storageTable = new StorageTable($mockTableGateway);

		$this->assertSame($resultSet, $storageTable->fetchAll());

	}

}

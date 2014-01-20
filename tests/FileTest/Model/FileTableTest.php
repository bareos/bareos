<?php

namespace FileTest\Model;

use File\Model\FileTable;
use File\Model\File;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class FileTableTest extends PHPUnit_Framework_TestCase 
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

		$fileTable = new FileTable($mockTableGateway);

		$this->assertSame($resultSet, $fileTable->fetchAll());

	}

}

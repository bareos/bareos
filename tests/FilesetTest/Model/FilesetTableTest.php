<?php

namespace FilesetTest\Model;

use Fileset\Model\FilesetTable;
use Fileset\Model\Fileset;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class FilesetTableTest extends PHPUnit_Framework_TestCase 
{
	
	public function testFetchAllReturnsAllFilesets() 
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

		$filesetTable = new FilesetTable($mockTableGateway);

		$this->assertSame($resultSet, $filesetTable->fetchAll());

	}

}

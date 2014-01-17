<?php

namespace MediaTest\Model;

use Media\Model\MediaTable;
use Media\Model\Media;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class MediaTableTest extends PHPUnit_Framework_TestCase 
{
	public function testFetchAllReturnsAllMedia() 
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


		$mediaTable = new MediaTable($mockTableGateway);

		$this->assertSame($resultSet, $mediaTable->fetchAll());

	}
}

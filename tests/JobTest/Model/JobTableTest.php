<?php

namespace JobTest\Model;

use Job\Model\JobTable;
use Job\Model\Job;
use Zend\Db\ResultSet\ResultSet;
use PHPUnit_Framework_TestCase;

class JobTableTest extends PHPUnit_Framework_TestCase
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

		$jobTable = new JobTable($mockTableGateway);

		$this->assertSame($resultSet, $jobTable->fetchAll());

	}

}

<?php

namespace PoolTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class PoolControllerTest extends AbstractHttpControllerTestCase
{
	protected $traceError = true;

	public function setUp()
	{
		$this->setApplicationConfig(
			include './config/application.config.php'	
		);
	}

	public function testIndexActionCanBeAccessed() 
	{
		$this->dispatch('/pool');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Pool');
		$this->assertControllerName('Pool\Controller\Pool');
		$this->assertControllerClass('PoolController');
		$this->assertMatchedRouteName('pool');
	}

}

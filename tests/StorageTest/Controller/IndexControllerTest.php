<?php

namespace StorageTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class StorageControllerTest extends AbstractHttpControllerTestCase 
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
		$this->dispatch('/storage');	
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Storage');
		$this->assertControllerName('Storage\Controller\Storage');
		$this->assertControllerClass('StorageController');
		$this->assertMatchedRouteName('storage');
	}

}

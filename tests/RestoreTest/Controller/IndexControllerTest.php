<?php

namespace RestoreTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class RestoreControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/restore');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Restore');
		$this->assertControllerName('Restore\Controller\Restore');
		$this->assertControllerClass('RestoreController');
		$this->assertMatchedRouteName('restore');
	}

}

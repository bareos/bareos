<?php

namespace AdminTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class AdminControllerTest extends AbstractHttpControllerTestCase 
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
		$this->dispatch('/admin');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Admin');
		$this->assertControllerName('Admin\Controller\Admin');
		$this->assertControllerClass('AdminController');
		$this->assertMatchedRouteName('admin');
	}

}

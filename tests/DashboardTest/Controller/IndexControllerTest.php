<?php

namespace DashboardTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class DashboardControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/dashboard');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Dashboard');
		$this->assertControllerName('Dashboard\Controller\Dashboard');
		$this->assertControllerClass('DashboardController');
		$this->assertMatchedRouteName('dashboard');
	}

}

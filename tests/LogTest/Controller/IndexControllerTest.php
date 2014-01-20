<?php

namespace LogTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class LogControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/log');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Log');
		$this->assertControllerName('Log\Controller\Log');
		$this->assertControllerClass('LogController');
		$this->assertMatchedRouteName('log');
	}

}

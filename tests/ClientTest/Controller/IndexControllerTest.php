<?php

namespace ClientTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class ClientControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/client');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Client');
		$this->assertControllerName('Client\Controller\Client');
		$this->assertControllerClass('ClientController');
		$this->assertMatchedRouteName('client');
	}

}

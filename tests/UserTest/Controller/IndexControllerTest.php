<?php

namespace UserTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class UserControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/user');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('User');
		$this->assertControllerName('User\Controller\User');
		$this->assertControllerClass('UserController');
		$this->assertMatchedRouteName('user');
	}

}

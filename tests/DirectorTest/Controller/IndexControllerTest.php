<?php

namespace DirectorTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class DirectorControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/director');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Director');
		$this->assertControllerName('Director\Controller\Director');
		$this->assertControllerClass('DirectorController');
		$this->assertMatchedRouteName('director');
	}

}

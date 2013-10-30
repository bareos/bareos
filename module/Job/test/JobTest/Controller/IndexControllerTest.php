<?php

namespace JobTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class JobControllerTest extends AbstractHttpControllerTestCase 
{

	protected $traceError = true;

	public function setUp()
	{
		$this->setApplicationConfig(
			include '/srv/www/htdocs/barbossa/config/application.config.php'		
		);
	}

	public function testIndexActionCanBeAccessed() 
	{
		$this->dispatch('/job');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Job');
		$this->assertControllerName('Job\Controller\Job');
		$this->assertControllerClass('JobController');
		$this->assertMatchedRouteName('job');
	}

}

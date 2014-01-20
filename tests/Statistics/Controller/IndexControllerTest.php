<?php

namespace StatisticsTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class StatisticsControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/statistics');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Statistics');
		$this->assertControllerName('Statistics\Controller\Statistics');
		$this->assertControllerClass('StatisticsController');
		$this->assertMatchedRouteName('statistics');
	}

}

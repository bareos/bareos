<?php

namespace MediaTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class MediaControllerTest extends AbstractHttpControllerTestCase 
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
		$this->dispatch('/media');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Media');
		$this->assertControllerName('Media\Controller\Media');
		$this->assertControllerClass('MediaController');
		$this->assertMatchedRouteName('media');
	}

}

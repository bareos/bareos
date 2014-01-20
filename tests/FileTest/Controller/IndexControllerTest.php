<?php

namespace FileTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class FileControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/file');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('File');
		$this->assertControllerName('File\Controller\File');
		$this->assertControllerClass('FileController');
		$this->assertMatchedRouteName('file');
	}

}

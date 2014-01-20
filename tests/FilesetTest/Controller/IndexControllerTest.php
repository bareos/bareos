<?php

namespace FilesetTest\Controller;

use Zend\Test\PHPUnit\Controller\AbstractHttpControllerTestCase;

class FilesetControllerTest extends AbstractHttpControllerTestCase
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
		$this->dispatch('/fileset');
		$this->assertResponseStatusCode(200);
		$this->assertModuleName('Fileset');
		$this->assertControllerName('Fileset\Controller\Fileset');
		$this->assertControllerClass('FilesetController');
		$this->assertMatchedRouteName('fileset');
	}

}

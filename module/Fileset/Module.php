<?php

namespace Fileset;

use Fileset\Model\Fileset;
use Fileset\Model\FilesetTable;
use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;

class Module
{

	public function getAutoloaderConfig()
	{
		return array(
			'Zend\Loader\ClassMapAutoloader' => array(
				__DIR__ . '/autoload_classmap.php',
			),
			'Zend\Loader\StandardAutoloader' => array(
				'namespaces' => array(
					__NAMESPACE__ => __DIR__ . '/src/' . __NAMESPACE__,
				),
			),
		);
	}

	public function getConfig()
	{
		return include  __DIR__ . '/config/module.config.php';
	}

	public function getServiceConfig()
	{
		return array(
			'factories' => array(
				'Fileset\Model\FilesetTable' => function($sm) {
					$tableGateway = $sm->get('FilesetTableGateway');
					$table = new FilesetTable($tableGateway);
					return $table;
				},
				'FilesetTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Fileset());
					return new TableGateway('fileset', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


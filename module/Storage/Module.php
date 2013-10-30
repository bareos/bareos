<?php

namespace Storage;

use Storage\Model\Storage;
use Storage\Model\StorageTable;
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
				'Storage\Model\StorageTable' => function($sm) {
					$tableGateway = $sm->get('StorageTableGateway');
					$table = new StorageTable($tableGateway);
					return $table;
				},
				'StorageTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Storage());
					return new TableGateway('storage', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


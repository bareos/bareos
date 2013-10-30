<?php

namespace Volume;

use Volume\Model\Volume;
use Volume\Model\VolumeTable;
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
				'Volume\Model\VolumeTable' => function($sm) {
					$tableGateway = $sm->get('VolumeTableGateway');
					$table = new VolumeTable($tableGateway);
					return $table;
				},
				'VolumeTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Volume());
					return new TableGateway('volume', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


<?php

namespace Pool;

use Pool\Model\Pool;
use Pool\Model\PoolTable;
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
				'Pool\Model\PoolTable' => function($sm) {
					$tableGateway = $sm->get('PoolTableGateway');
					$table = new PoolTable($tableGateway);
					return $table;
				},
				'PoolTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Pool());
					return new TableGateway('pool', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


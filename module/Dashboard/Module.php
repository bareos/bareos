<?php

namespace Dashboard;

use Dashboard\Model\Dashboard;
use Dashboard\Model\DashboardTable;
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
				'Dashboard\Model\DashboardTable' => function($sm) {
					$tableGateway = $sm->get('DashboardTableGateway');
					$table = new DashboardTable($tableGateway);
					return $table;
				},
				'DashboardTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Dashboard());
					return new TableGateway('dashboard', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


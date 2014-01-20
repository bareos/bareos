<?php

namespace Statistics;

use Statistics\Model\Statistics;
use Statistics\Model\StatisticsTable;
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
		return include __DIR__ . '/config/module.config.php';
	}

	public function getServiceConfig() 
	{
		return array(
			'factories' => array(
				'Statistics\Model\StatisticsTable' => function($sm) 
				{
					$tableGateway = $sm->get('StatisticsTableGateway');
					$table = new StatisticsTable($tableGateway);
					return $table;
				},
				'StatisticsTableGateway' => function($sm)
				{
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Statistics());
					return new TableGateway('statistics', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


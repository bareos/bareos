<?php

namespace Log;

use Log\Model\Log;
use Log\Model\LogTable;
use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;

class Module 
{

	public function getServiceConfig()
	{
		return array(
			'factories' => array(
				'Log\Model\LogTable' => function($sm) {
					$tableGateway = $sm->get('LogTableGateway');
					$table = new LogTable($tableGateway);
					return $table;
				},
				'LogTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Log());
					return new TableGateway('log', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

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

}

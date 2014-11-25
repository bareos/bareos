<?php

namespace Director;

use Director\Model\Director;
use Director\Model\DirectorTable;
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
				'Director\Model\DirectorTable' => function($sm)
				{
					$tableGateway = $sm->get('DirectorTableGateway');
					$table = new DirectorTable($tableGateway);
					return $table;
				},
				'DirectorTableGateway' => function($sm)
				{
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Director());
					return new TableGateway('director', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


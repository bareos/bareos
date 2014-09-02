<?php

namespace Install;

use Install\Model\Install;
use Install\Model\InstallTable;
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
				'Install\Model\InstallTable' => function($sm) 
				{
					$tableGateway = $sm->get('InstallTableGateway');
					$table = new InstallTable($tableGateway);
					return $table;
				},
				'InstallTableGateway' => function($sm)
				{
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Install());
					return new TableGateway('install', $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


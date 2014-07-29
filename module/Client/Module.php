<?php

namespace Client;

use Client\Model\Client;
use Client\Model\ClientTable;
use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Bareos\Db\Sql\BareosSqlCompatHelper;

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
				'Client\Model\ClientTable' => function($sm) {
					$tableGateway = $sm->get('ClientTableGateway');
					$table = new ClientTable($tableGateway);
					return $table;
				},
				'ClientTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Client());
					$config = $sm->get('Config');
					$bsqlch = new BareosSqlCompatHelper($config['db']['driver']);
					return new TableGateway($bsqlch->strdbcompat("Client"), $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


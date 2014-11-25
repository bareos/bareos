<?php

namespace Media;

use Media\Model\Media;
use Media\Model\MediaTable;
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
				'Media\Model\MediaTable' => function($sm) {
					$tableGateway = $sm->get('MediaTableGateway');
					$table = new MediaTable($tableGateway);
					return $table;
				},
				'MediaTableGateway' => function($sm) {
					//$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$dbAdapter = $sm->get($_SESSION['bareos']['director']);
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Media());
					$config = $sm->get('Config');
					$bsqlch = new BareosSqlCompatHelper($config['db']['adapters'][$_SESSION['bareos']['director']]['driver']);
					return new TableGateway($bsqlch->strdbcompat("Media"), $dbAdapter, null, $resultSetPrototype);
				},
			),
		);
	}

}


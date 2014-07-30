<?php

namespace Job;

use Job\Model\Job;
use Job\Model\JobTable;
use Zend\Db\ResultSet\ResultSet;
use Zend\Db\TableGateway\TableGateway;
use Bareos\Db\Sql\BareosSqlCompatHelper;

class Module 
{

	public function getServiceConfig()
	{
		return array(
			'factories' => array(
				'Job\Model\JobTable' => function($sm) {
					$tableGateway = $sm->get('JobTableGateway');
					$table = new JobTable($tableGateway);
					return $table;
				},
				'JobTableGateway' => function($sm) {
					$dbAdapter = $sm->get('Zend\Db\Adapter\Adapter');
					$resultSetPrototype = new ResultSet();
					$resultSetPrototype->setArrayObjectPrototype(new Job());
					$config = $sm->get('Config');
					$bsqlch = new BareosSqlCompatHelper($config['db']['driver']);
					return new TableGateway($bsqlch->strdbcompat("Job"), $dbAdapter, null, $resultSetPrototype);
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

<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Statistics\Controller\Statistics' => 'Statistics\Controller\StatisticsController',
		),
	),
	
	'router' => array(
		'routes' => array(
			'statistics' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/statistics[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Statistics\Controller\Statistics',
						'action' => 'index',
					),
				),
			),
		),
	),
	
	'view_manager' => array(
		'template_path_stack' => array(
			'statistics' => __DIR__ . '/../view',
		),
	),

);

<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Pool\Controller\Pool' => 'Pool\Controller\PoolController',
		),
	),

	'router' => array(
		'routes' => array(
			'pool' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/pool[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Pool\Controller\Pool',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'pool' => __DIR__ . '/../view',
		),
	),

);

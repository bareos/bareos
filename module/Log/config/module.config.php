<?php

return array(

	'controllers' => array(
			'invokables' => array(
					'Log\Controller\Log' => 'Log\Controller\LogController',
				),
	),

	'router' => array(
		'routes' => array(
			'log' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/log[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Log\Controller\Log',
						'action' => 'index',
					),
				),
			),
		),
	),

	'view_manager' => array(
			'template_path_stack' => array(
					'log' => __DIR__ . '/../view',
				),
	),

);


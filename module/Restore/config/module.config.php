<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Restore\Controller\Restore' => 'Restore\Controller\RestoreController',
		),
	),
	
	'router' => array(
		'routes' => array(
			'restore' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/restore[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Restore\Controller\Restore',
						'action' => 'index',
					),
				),
			),
		),
	),
	
	'view_manager' => array(
		'template_path_stack' => array(
			'restore' => __DIR__ . '/../view',
		),
	),

);

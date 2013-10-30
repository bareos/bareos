<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Director\Controller\Director' => 'Director\Controller\DirectorController',
		),
	),
	
	'router' => array(
		'routes' => array(
			'director' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/director[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Director\Controller\Director',
						'action' => 'index',
					),
				),
			),
		),
	),
	
	'view_manager' => array(
		'template_path_stack' => array(
			'director' => __DIR__ . '/../view',
		),
	),

);

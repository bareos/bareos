<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Volume\Controller\Volume' => 'Volume\Controller\VolumeController',
		),
	),

	'router' => array(
		'routes' => array(
			'volume' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/volume[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Volume\Controller\Volume',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'volume' => __DIR__ . '/../view',
		),
	),

);

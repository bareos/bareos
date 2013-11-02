<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Fileset\Controller\Fileset' => 'Fileset\Controller\FilesetController',
		),
	),

	'router' => array(
		'routes' => array(
			'fileset' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/fileset[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Fileset\Controller\Fileset',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'fileset' => __DIR__ . '/../view',
		),
	),

);

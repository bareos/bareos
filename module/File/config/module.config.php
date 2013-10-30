<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'File\Controller\File' => 'File\Controller\FileController',
		),
	),

	'router' => array(
		'routes' => array(
			'file' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/file[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'File\Controller\File',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'file' => __DIR__ . '/../view',
		),
	),

);

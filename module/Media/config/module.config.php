<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Media\Controller\Media' => 'Media\Controller\MediaController',
		),
	),

	'router' => array(
		'routes' => array(
			'media' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/media[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Media\Controller\Media',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'media' => __DIR__ . '/../view',
		),
	),

);

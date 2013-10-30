<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Storage\Controller\Storage' => 'Storage\Controller\StorageController',
		),
	),

	'router' => array(
		'routes' => array(
			'storage' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/storage[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Storage\Controller\Storage',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'storage' => __DIR__ . '/../view',
		),
	),

);

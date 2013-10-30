<?php

return array(

	'controllers' => array(
		'invokables' => array(
			'Client\Controller\Client' => 'Client\Controller\ClientController',
		),
	),

	'router' => array(
		'routes' => array(
			'client' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/client[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Client\Controller\Client',
						'action' => 'index',
					),
				),
				
			),
		),
	),

	'view_manager' => array(
		'template_path_stack' => array(
			'client' => __DIR__ . '/../view',
		),
	),

);

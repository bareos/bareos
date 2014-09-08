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
					'route' => '/client[/][:action][/:id][order_by/:order_by][/:order][/][limit/:limit]',
					'constraints' => array(
						'action' => '(?!\blimit\b)(?!\border_by\b)[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
						'order_by' => '[a-zA-Z][a-zA-Z0-9_-]*',
                                                'order' => 'ASC|DESC',
                                                'limit' => '[0-9]+',
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

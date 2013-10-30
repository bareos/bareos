<?php

return array(

	'controllers' => array(
			'invokables' => array(
					'Job\Controller\Job' => 'Job\Controller\JobController',
				),
	),

	'router' => array(
		'routes' => array(
			'job' => array(
				'type' => 'segment',
				'options' => array(
					'route' => '/job[/][:action][/:id]',
					'constraints' => array(
						'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
						'id' => '[0-9]+',
					),
					'defaults' => array(
						'controller' => 'Job\Controller\Job',
						'action' => 'index',
					),
				),
			),
		),
	),

	'view_manager' => array(
			'template_path_stack' => array(
					'job' => __DIR__ . '/../view',
				),
	),

);


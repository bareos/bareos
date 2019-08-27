<?php

return array(
  'controllers' => array(
    'invokables' => array(
      'Console\Controller\Console' => 'Console\Controller\ConsoleController'
    )
  ),
  'controller_plugins' => array(
    'invokables' => array(
      'SessionTimeoutPlugin' => 'Application\Controller\Plugin\SessionTimeoutPlugin',
      'RequestURIPlugin' => 'Application\Controller\Plugin\RequestURIPlugin'
    )
  ),
  'router' => array(
    'routes' => array(
      'console' => array(
        'type' => 'segment',
        'options' => array(
          'route' => '/console[/][:action]',
          'constraints' => array(
            'action' => '[a-zA-Z][a-zA-Z0-9_-]*'
          ),
          'defaults' => array(
            'controller' => 'Console\Controller\Console',
            'action' => 'index'
          )
        )
      )
    )
  ),
  'view_manager' => array(
    'template_path_stack' => array(
      'console' => __DIR__ . '/../view'
    )
  )
);

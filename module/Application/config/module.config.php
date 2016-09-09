<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/ZendSkeletonApplication for the canonical source repository
 * @copyright Copyright (c) 2005-2013 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

return array(
    'router' => array(
   'routes' => array(
       'home' => array(
      'type' => 'Zend\Mvc\Router\Http\Literal',
      'options' => array(
          'route'    => '/',
          'defaults' => array(
         'controller' => 'Auth\Controller\Auth',
         'action'     => 'login',
          ),
      ),
       ),
       // The following is a route to simplify getting started creating
       // new controllers and actions without needing to create a new
       // module. Simply drop new controllers in, and you can access them
       // using the path /application/:controller/:action
       'application' => array(
      'type'    => 'Literal',
      'options' => array(
          'route'    => '/application',
          'defaults' => array(
         '__NAMESPACE__' => 'Application\Controller',
         'controller'    => 'Index',
         'action'   => 'index',
          ),
      ),
      'may_terminate' => true,
      'child_routes' => array(
          'default' => array(
         'type'    => 'Segment',
         'options' => array(
             'route'    => '/[:controller[/:action]]',
             'constraints' => array(
            'controller' => '[a-zA-Z][a-zA-Z0-9_-]*',
            'action'     => '[a-zA-Z][a-zA-Z0-9_-]*',
             ),
             'defaults' => array(
             ),
         ),
          ),
      ),
       ),
   ),
    ),
    'service_manager' => array(
   'abstract_factories' => array(
       'Zend\Cache\Service\StorageCacheAbstractServiceFactory',
       'Zend\Log\LoggerAbstractServiceFactory',
   ),
      'services' => array(
      ),
      'shared' => array(
      ),
      'factories' => array(
         'translator' => 'Zend\I18n\Translator\TranslatorServiceFactory',
         'navigation' => 'Zend\Navigation\Service\DefaultNavigationFactory',
         'director' => 'Bareos\BSock\BareosBSockServiceFactory',
      ),
   'aliases' => array(
   ),
      'invokables' => array(
      ),
    ),
   'translator' => array(
      'locale' => 'en_EN',
      'translation_file_patterns' => array(
          array(
            'type'     => 'gettext',
            'base_dir' => __DIR__ . '/../language',
            'pattern'  => '%s.mo',
         ),
      ),
   ),
   'controllers' => array(
      'invokables' => array(
         'Application\Controller\Index' => 'Application\Controller\IndexController',
      ),
   ),
   'view_helpers' => array(
      'invokables' => array (
         //'printExample' => 'Application\View\Helper\Example', // Example ViewHelper
         'UpdateAlert' => 'Application\View\Helper\UpdateAlert',
      ),
   ),
    'view_manager' => array(
   'display_not_found_reason' => true,
   'display_exceptions'       => true,
   'doctype'        => 'HTML5',
   'not_found_template'       => 'error/404',
   'exception_template'       => 'error/index',
   'template_map' => array(
       'layout/layout'      => __DIR__ . '/../view/layout/layout.phtml',
       'application/index/index' => __DIR__ . '/../view/application/index/index.phtml',
       'error/404'          => __DIR__ . '/../view/error/404.phtml',
       'error/index'        => __DIR__ . '/../view/error/index.phtml',
   ),
   'template_path_stack' => array(
       __DIR__ . '/../view',
   ),
      'strategies' => array(
         'ViewJsonStrategy',
      ),
    ),
    // Placeholder for console routes
    'console' => array(
   'router' => array(
       'routes' => array(
       ),
   ),
    ),
    'navigation' => array(
      'default' => array(
         array(
            'label' => _('Dashboard'),
            'route' => 'dashboard',
         ),
         array(
            'label' => _('Jobs'),
            'route' => 'job',
            'pages' => array(
               array(
                  'label' => _('Overview'),
                  'route' => 'job',
                  'action' => 'index'
               ),
               array(
                  'label' => _('Run'),
                  'route' => 'job',
                  'action' => 'run'
               ),
            ),
         ),
         array(
            'label' => _('Restore'),
            'route' => 'restore',
         ),
         array(
            'label' => _('Clients'),
            'route' => 'client',
         ),
         array(
            'label' => _('Schedules'),
            'route' => 'schedule',
         ),
         array(
            'label' => _('Storages'),
            'route' => 'storage',
            'pages' => array(
               array(
                  'label' => _('Pool'),
                  'route' => 'pool',
                  'action' => 'index'
               ),
               array(
                  'label' => _('Pool'),
                  'route' => 'pool',
                  'action' => 'details'
               ),
               array(
                  'label' => _('Volumes'),
                  'route' => 'media',
                  'action' => 'index'
               ),
               array(
                  'label' => _('Volumes'),
                  'route' => 'media',
                  'action' => 'details'
               ),
            ),
         ),
         array(
            'label' => _('Director'),
            'route' => 'director',
         ),
      ),
   ),
);


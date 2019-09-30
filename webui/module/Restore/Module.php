<?php

namespace Restore;

use Restore\Model\Restore;
use Restore\Model\RestoreModel;

class Module
{

  public function getAutoloaderConfig()
  {
    return array(
      'Zend\Loader\ClassMapAutoloader' => array(
        __DIR__ . '/autoload_classmap.php',
      ),
      'Zend\Loader\StandardAutoloader' => array(
        'namespaces' => array(
          __NAMESPACE__ => __DIR__ . '/src/' . __NAMESPACE__,
        ),
      ),
    );
  }

  public function getConfig()
  {
    return include __DIR__ . '/config/module.config.php';
  }

  public function getServiceConfig()
  {
    return array(
      'factories' => array(
        'Restore\Model\RestoreModel' => function()
        {
          $model = new RestoreModel();
          return $model;
        },
      ),
    );
  }

}


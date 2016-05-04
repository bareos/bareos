<?php

namespace Application\Controller\Plugin;

use Zend\Mvc\Controller\Plugin\AbstractPlugin;

class SessionTimeoutPlugin extends AbstractPlugin
{

   public function timeout()
   {
      // preparation for setting timeout via config file:
      $this->getController()->getServiceLocator()->get('config');
      $timeout = 3600;

      if($_SESSION['bareos']['idletime'] + $timeout > time()) {
         $_SESSION['bareos']['idletime'] = time();
         return true;
      }
      else {
         session_destroy();
         return false;
      }
   }

}

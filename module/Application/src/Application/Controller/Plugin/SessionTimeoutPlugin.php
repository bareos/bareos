<?php

namespace Application\Controller\Plugin;

use Zend\Mvc\Controller\Plugin\AbstractPlugin;

class SessionTimeoutPlugin extends AbstractPlugin
{

   public function timeout()
   {
      // preparation for setting timeout via config file:
      $config = $this->getController()->getServiceLocator()->get('config');

      $timeout = 3600;

      if($timeout === 0) {
         return true;
      }
      else {
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

   public function isValid()
   {
      if($_SESSION['bareos']['authenticated']) {
         if($this->timeout()) {
            return true;
         }
         else {
            return false;
         }
      }
      else {
         return false;
      }
   }
}

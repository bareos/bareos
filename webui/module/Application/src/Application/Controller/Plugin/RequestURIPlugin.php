<?php

namespace Application\Controller\Plugin;

use Zend\Mvc\Controller\Plugin\AbstractPlugin;

class RequestURIPlugin extends AbstractPlugin
{
   private $requestURI;

   public function setRequestURI()
   {
      $r = "";
      if(array_key_exists('REQUEST_URI', $_SERVER)) {
         $r = $_SERVER['REQUEST_URI'];
      }
      $this->requestURI = $r;
   }

   public function getRequestURI()
   {
      return $this->requestURI;
   }
}

<?php

namespace Application\Controller\Plugin;

use Zend\Mvc\Controller\Plugin\AbstractPlugin;

class CommandACLPlugin extends AbstractPlugin
{
   private $commands = null;
   private $required = null;

   public function validate($commands=null, $required=null)
   {
      $this->commands = $commands;
      $this->required = $required;

      foreach($this->required as $cmd) {
         if($this->commands[$cmd]['permission'] == 0) {
            return false;
         }
      }
      return true;
   }
}

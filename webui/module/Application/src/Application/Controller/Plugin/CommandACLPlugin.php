<?php

namespace Application\Controller\Plugin;

use Zend\Mvc\Controller\Plugin\AbstractPlugin;

class CommandACLPlugin extends AbstractPlugin
{
  /**
   * Get a list of invalid bconsole commands as an array
   *
   * Compares an array of bconsole commands with available commands given by
   * ACL settings and returns the commands which are invalid due to
   * restrictions by the Command ACL directive in console/profile
   * resource settings as an array.
   *
   * Note: The list of available commands given by Command ACL settings is
   * requested by the .help command during login time in the Auth module
   * and stored in $_SESSION['bareos']['commands'].
   *
   * @param commands
   *
   * @return array
   */
  public function getInvalidCommands($commands=null)
  {
    if(is_array($commands) && !empty($commands)) {
      $unknown_commands = array();
      foreach($commands as $command) {
        if(array_key_exists($command, $_SESSION['bareos']['commands'])) {
          if(!$_SESSION['bareos']['commands'][$command]['permission']) {
            array_push($unknown_commands, $command);
          }
        } else {
          array_push($unknown_commands, $command);
        }
      }
      return $unknown_commands;
    }
    return null;
  }
}

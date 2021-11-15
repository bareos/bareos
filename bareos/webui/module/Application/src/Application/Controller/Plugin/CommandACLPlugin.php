<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2019 Bareos GmbH & Co. KG (http://www.bareos.org/)
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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
      $invalid_commands = array();
      foreach($commands as $command) {
        if(array_key_exists($command, $_SESSION['bareos']['commands'])) {
          if(!$_SESSION['bareos']['commands'][$command]['permission']) {
            array_push($invalid_commands, $command);
          }
        } else {
          array_push($invalid_commands, $command);
        }
      }
      return $invalid_commands;
    }
    return null;
  }
}

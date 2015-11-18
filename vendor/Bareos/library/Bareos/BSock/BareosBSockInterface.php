<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2014-2015 Bareos GmbH & Co. KG
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

namespace Bareos\BSock;

interface BareosBSockInterface
{
   /**
    * Initialize connection
    *
    * @return boolean
    */
   public function init();

   /**
    * Authenticate
    *
    * @param $console
    * @param $password
    * @return boolean
    */
   public function auth($console, $password);

   /**
    * Set user credentials
    *
    * @param $username
    * @param password
    */
   public function set_user_credentials($username=null, $password=null);

   /**
    * Set configuration
    *
    * @param $config
    */
   public function set_config($config);

   /**
    * Disconnect
    *
    * @return boolean
    */
   public function disconnect();

   /**
    * Send command
    *
    * @param $cmd
    * @return string
    */
   public function send_command($cmd);

}

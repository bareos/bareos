<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
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

use Zend\ServiceManager\FactoryInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class BareosBSockServiceFactory implements FactoryInterface
{
   protected $bsock;

   /**
    */
   public function createService(ServiceLocatorInterface $serviceLocator)
   {
      $config = $serviceLocator->get('Config');
      $this->bsock = new BareosBSock($config['directors']);

      if (isset($_SESSION['bareos']['director'])) {
         $this->bsock->set_config($config['directors'][$_SESSION['bareos']['director']]);
         $this->bsock->set_user_credentials($_SESSION['bareos']['username'], $_SESSION['bareos']['password']);
         $this->bsock->connect_and_authenticate();
      }

      return $this->bsock;
   }
}

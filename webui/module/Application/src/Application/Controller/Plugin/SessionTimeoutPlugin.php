<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2020 Bareos GmbH & Co. KG (http://www.bareos.org/)
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
use Zend\Session\Container;

class SessionTimeoutPlugin extends AbstractPlugin
{
   protected $session = null;

   public function timeout()
   {
      $configuration = $this->getController()->getServiceLocator()->get('config');
      $timeout = $configuration['configuration']['session']['timeout'];

      if($timeout === 0) {
         return false;
      } else {
         if(($this->session->offsetGet('idletime') + $timeout) > time()) {
            $this->session->offsetSet('idletime', time());
            return false;
         } else {
            $this->session->getManager()->destroy();
            return true;
         }
      }
   }

   public function isValid()
   {
      $this->session = new Container('bareos');

      if($this->session->offsetGet('authenticated')) {
         if($this->timeout()) {
            return false;
         } else {
            return true;
         }
      } else {
         return false;
      }
   }

}

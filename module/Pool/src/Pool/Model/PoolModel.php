<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Pool\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class PoolModel implements ServiceLocatorAwareInterface
{
   protected $serviceLocator;
   protected $director;

   public function __construct()
   {
   }

   public function setServiceLocator(ServiceLocatorInterface $serviceLocator)
   {
      $this->serviceLocator = $serviceLocator;
   }

   public function getServiceLocator()
   {
      return $this->serviceLocator;
   }

   public function getPools()
   {
      $cmd = 'llist pools';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $pools = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $pools['result']['pools'];
   }

   public function getPool($pool=null)
   {
      if(isset($pool)) {
         $cmd = 'llist pool="'.$pool.'"';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $pool = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $pool['result']['pools'];
      }
      else {
         return false;
      }
   }

   public function getPoolMedia($pool=null)
   {
      if(isset($pool)) {
         $cmd = 'llist media pool="'.$pool.'"';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $media = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $media['result']['volumes'];
      }
      else {
         return false;
      }
   }
}

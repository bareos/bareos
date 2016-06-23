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

namespace Client\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class ClientModel implements ServiceLocatorAwareInterface
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

   public function getClients()
   {
      $cmd = 'llist clients';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $clients['result']['clients'];
   }

   public function getClient($client=null)
   {
      if(isset($client)) {
         $cmd = 'llist client="'.$client.'"';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         $client = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $client['result']['clients'];
      }
      else {
         return false;
      }
   }

   public function getClientBackups($client=null, $limit=null, $order=null)
   {
      if(isset($client)) {
         if(isset($limit, $order)) {
            $cmd = 'llist backups client="'.$client.'" limit='.$limit.' order='.$order.'';
         }
         else {
            $cmd = 'llist backups client="'.$client.'"';
         }
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 2, null);
         if(preg_match("/Select/", $result)) {
            return null;
         }
         else {
            $backups = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $backups['result']['backups'];
         }
      }
      else {
         return false;
      }
   }

   public function statusClient($name=null)
   {
      if(isset($name)) {
         $cmd = 'status client="'.$name;
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function enableClient($name=null)
   {
      if(isset($name)) {
         $cmd = 'enable client="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function disableClient($name=null)
   {
      if(isset($name)) {
         $cmd = 'disable client="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }
}

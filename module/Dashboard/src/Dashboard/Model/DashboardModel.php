<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2014 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Dashboard\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;
use Zend\Json\Json;

class DashboardModel implements ServiceLocatorAwareInterface
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

   public function getJobs($status=null, $days=null, $hours=null)
   {
      if(isset($status)) {
         $this->director = $this->getServiceLocator()->get('director');
         if(isset($days)) {
            $cmd = 'llist jobs jobstatus='.$status.' days='.$days.'';
         }
         elseif(isset($hours)) {
            $cmd = 'llist jobs jobstatus='.$status.' hours='.$hours.'';
         }
         else {
            $cmd = 'llist jobs jobstatus='.$status.'';
         }
         $result = $this->director->send_command($cmd, 2, null);
         $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $jobs['result']['jobs'];
      }
      else {
         return false;
      }
   }

   public function getJobsLastStatus()
   {
      $cmd = 'llist jobs last';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $jobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $jobs['result']['jobs'];
   }

   public function getLastDirectorMessages()
   {
      $cmd = 'llist log limit=50';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $msg = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $msg['result']['log'];
   }

}

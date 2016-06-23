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

namespace Schedule\Model;

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class ScheduleModel implements ServiceLocatorAwareInterface
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

   public function getSchedules()
   {
      $cmd = '.schedule';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $schedules = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $schedules['result']['schedules'];
   }

   public function showSchedules()
   {
      $cmd = 'show schedule';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function getFullScheduleStatus()
   {
      $cmd = 'status scheduler';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function getScheduleStatus($name=null)
   {
      if(isset($name)) {
         $cmd = 'status scheduler schedule="'.$name;
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function enableSchedule($name=null)
   {
      if(isset($name)) {
         $cmd = 'enable schedule="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }

   public function disableSchedule($name=null)
   {
      if(isset($name)) {
         $cmd = 'disable schedule="'.$name.'" yes';
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }
}

<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2017 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

class ScheduleModel
{

   /**
    * Get Schedules
    *
    * @param $bsock
    *
    * @return array
    */
   public function getSchedules(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = '.schedule';
         $result = $bsock->send_command($cmd, 2);
         $schedules = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $schedules['result']['schedules'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Show Schedules
    *
    * @param $bsock
    *
    * @return string
    */
   public function showSchedules(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'show schedule';
         $result = $bsock->send_command($cmd, 0);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Full Schedule Status
    *
    * @param $bsock
    *
    * @return string
    */
   public function getFullScheduleStatus(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'status scheduler';
         $result = $bsock->send_command($cmd, 0);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Schedule Status
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function getScheduleStatus(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'status scheduler schedule="'.$name;
         $result = $bsock->send_command($cmd, 0);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Enable Schedule
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function enableSchedule(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'enable schedule="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Disable Schedule
    *
    * @param $bsock
    * @param $name
    *
    * @return string
    */
   public function disableSchedule(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'disable schedule="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

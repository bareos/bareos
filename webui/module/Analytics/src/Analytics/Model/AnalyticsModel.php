<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2013-2022 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Analytics\Model;

class AnalyticsModel
{
  public function getJobTotals(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list jobtotals';
         $result = $bsock->send_command($cmd, 2);
         $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $children = array("children" => $jobtotals['result']['jobs']);
         return $children;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getOverallJobTotals(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list jobtotals';
         $result = $bsock->send_command($cmd, 2);
         $jobtotals = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $result = $jobtotals['result']['jobtotals'];
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getRunningJobs(&$bsock)
   {
      if(isset($bsock)) {
         //$cmd = 'llist jobs jobstatus=R';
         $cmd = 'status running';
         $result = $bsock->send_command($cmd, 2);
         $runningjobs = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         $result = $runningjobs['result']['jobs'];
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

}

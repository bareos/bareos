<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2016 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Director\Model;

class DirectorModel
{
   public function getDirectorVersion(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'version';
         $result = $bsock->send_command($cmd, 2, null);
         $version = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $version['result']['version'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getDirectorStatus(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'status director';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getDirectorMessages(&$bsock=null, $limit=null, $offset=null, $reverse=null)
   {
      if(isset($bsock, $limit)) {
         if($reverse && $offset == null) {
            $cmd = 'list log limit='.$limit.' reverse';
         }
         else if($reverse && $offset != null) {
            $cmd = 'list log limit='.$limit.' offset='.$offset.' reverse';
         }
         else if(!$reverse && $offset != null) {
            $cmd = 'list log limit='.$limit.' offset='.$offset;
         }
         else if(!$reverse && $offset == null) {
            $cmd = 'list log limit='.$limit;
         }
         $result = $bsock->send_command($cmd, 2, null);
         $messages = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $messages['result']['log'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function sendCommand(&$bsock=null, $cmd=null)
   {
      if(isset($bsock, $cmd)) {
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

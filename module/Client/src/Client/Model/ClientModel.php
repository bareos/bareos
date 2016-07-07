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

namespace Client\Model;

class ClientModel
{

   public function getClients(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist clients';
         $result = $bsock->send_command($cmd, 2, null);
         $clients = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $clients['result']['clients'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getClient(&$bsock=null, $client=null)
   {
      if(isset($bsock, $client)) {
         $cmd = 'llist client="'.$client.'"';
         $result = $bsock->send_command($cmd, 2, null);
         $client = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $client['result']['clients'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getClientBackups(&$bsock=null, $client=null, $limit=null, $order=null)
   {
      if(isset($bsock, $client)) {
         if(isset($limit, $order)) {
            $cmd = 'llist backups client="'.$client.'" limit='.$limit.' order='.$order.'';
         }
         else {
            $cmd = 'llist backups client="'.$client.'"';
         }
         $result = $bsock->send_command($cmd, 2, null);
         if(preg_match("/Select/", $result)) {
            return null;
         }
         else {
            $backups = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $backups['result']['backups'];
         }
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function statusClient(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'status client="'.$name;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function enableClient(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'enable client="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function disableClient(&$bsock=null, $name=null)
   {
      if(isset($bsock, $name)) {
         $cmd = 'disable client="'.$name.'" yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

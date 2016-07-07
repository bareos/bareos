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

namespace Pool\Model;

class PoolModel
{

   public function getPools(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist pools';
         $result = $bsock->send_command($cmd, 2, null);
         $pools = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $pools['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getPool(&$bsock=null, $pool=null)
   {
      if(isset($bsock, $pool)) {
         $cmd = 'llist pool="'.$pool.'"';
         $result = $bsock->send_command($cmd, 2, null);
         $pool = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $pool['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getPoolMedia(&$bsock=null, $pool=null)
   {
      if(isset($bsock, $pool)) {
         $cmd = 'llist media pool="'.$pool.'"';
         $result = $bsock->send_command($cmd, 2, null);
         $media = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $media['result']['volumes'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

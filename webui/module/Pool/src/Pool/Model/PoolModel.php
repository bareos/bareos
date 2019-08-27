<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
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

namespace Pool\Model;

class PoolModel
{

   /**
    * Get all Pools by llist command
    *
    * @param $bsock
    *
    * @return array
    */
   public function getPools(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist pools';
         $result = $bsock->send_command($cmd, 2);
         $pools = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $pools['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get all Pools by .pools command
    *
    * @param $bsock
    * @param $type
    *
    * @return array
    */
   public function getDotPools(&$bsock=null, $type=null)
   {
      if(isset($bsock)) {
         if($type == null) {
            $cmd = '.pools';
         }
         else {
            $cmd = '.pools type="'.$type.'"';
         }
         $pools = $bsock->send_command($cmd, 2);
         $result = \Zend\Json\Json::decode($pools, \Zend\Json\Json::TYPE_ARRAY);
         return $result['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get a single Pool
    *
    * @param $bsock
    * @param $pool
    *
    * @return array
    */
   public function getPool(&$bsock=null, $pool=null)
   {
      if(isset($bsock, $pool)) {
         $cmd = 'llist pool="'.$pool.'"';
         $result = $bsock->send_command($cmd, 2);
         $pool = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $pool['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Pool Media by llist media command
    *
    * @param $bsock
    * @param $pool
    *
    * @return array
    */
   public function getPoolMedia(&$bsock=null, $pool=null)
   {
      if(isset($bsock, $pool)) {
         $cmd = 'llist media pool="'.$pool.'"';
         $limit = 1000;
         $offset = 0;
         $retval = array();
         while (true) {
            $result = $bsock->send_command($cmd . ' limit=' . $limit . ' offset=' . $offset, 2);
            if (preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
               $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
               return $error['result']['error'];
            } else {
               $media = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
               if ( empty($media['result']) ) {
                  return false; // No matching records found
               }
               if ( empty($media['result']['volumes']) && $media['result']['meta']['range']['filtered'] === 0 ) {
                  return $retval;
               } else {
                  $retval = array_merge($retval, $media['result']['volumes']);
               }
            }
            $offset = $offset + $limit;
         }
      } else {
         throw new \Exception('Missing argument.');
      }
   }
}

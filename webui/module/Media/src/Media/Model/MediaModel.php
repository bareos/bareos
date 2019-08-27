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

namespace Media\Model;

class MediaModel
{

   /**
    * Get all Volumes/Media
    *
    * @param $bsock
    *
    * @return array
    */
   public function getVolumes(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist volumes all';
         $limit = 1000;
         $offset = 0;
         $retval = array();
         while (true) {
            $result = $bsock->send_command($cmd . ' limit=' . $limit . ' offset=' . $offset, 2);
            if (preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
               $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
               return $error['result']['error'];
            } else {
               $volumes = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
               if ( empty($volumes['result']) ) {
                  return false; // No matching records found
               }
               if ( empty($volumes['result']['volumes']) && $volumes['result']['meta']['range']['filtered'] === 0 ) {
                  return $retval;
               } else {
                  $retval = array_merge($retval, $volumes['result']['volumes']);
               }
            }
            $offset = $offset + $limit;
         }
      } else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get a single Volume
    *
    * @param $bsock
    * @param $volume
    *
    * @return array
    */
   public function getVolume(&$bsock=null, $volume=null)
   {
      if(isset($bsock, $volume)) {
         $cmd = 'llist volume="'.$volume.'"';
         $result = $bsock->send_command($cmd, 2);
         $volume = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $volume['result']['volume'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get Volume Jobs
    *
    * @param $bsock
    * @param $volume
    *
    * @return array
    */
   public function getVolumeJobs(&$bsock=null, $volume=null)
   {
      if(isset($bsock, $volume)) {
         $cmd = 'llist jobs volume="'.$volume.'"';
         $result = $bsock->send_command($cmd, 2);
         if(preg_match('/Failed to send result as json. Maybe result message to long?/', $result)) {
            $error = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $error['result']['error'];
         }
         else {
            $volume = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
            return $volume['result']['jobs'];
         }
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

}

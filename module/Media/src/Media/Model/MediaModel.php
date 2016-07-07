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

namespace Media\Model;

class MediaModel
{

   public function getVolumes(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'llist volumes all';
         $result = $bsock->send_command($cmd, 2, null);
         $volumes = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $volumes['result']['volumes'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getVolume(&$bsock=null, $volume=null)
   {
      if(isset($bsock, $volume)) {
         $cmd = 'llist volume="'.$volume.'"';
         $result = $bsock->send_command($cmd, 2, null);
         $volume = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $volume['result']['volume'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getVolumeJobs(&$bsock=null, $volume=null)
   {
      if(isset($bsock, $volume)) {
         $cmd = 'llist jobs volume="'.$volume.'"';
         $result = $bsock->send_command($cmd, 2, null);
         $volume = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $volume['result']['jobs'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

}

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

namespace Fileset\Model;

class FilesetModel
{

   /**
    * Get all Filesets
    *
    * @param $bsock
    *
    * @return array
    */
   public function getFilesets(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list filesets';
         $result = $bsock->send_command($cmd, 2, null);
         $filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $filesets['result']['filesets'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get a single Fileset
    *
    * @param $bsock
    * @param $id
    *
    * @return array
    */
   public function getFileset(&$bsock=null, $id)
   {
      if(isset($bsock, $id)) {
         $cmd = 'llist fileset filesetid='.$id.'';
         $result = $bsock->send_command($cmd, 2, null);
         $fileset = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $fileset['result']['filesets'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   /**
    * Get all Filesets by .filesets command
    *
    * @param $bsock
    *
    * @return array
    */
   public function getDotFilesets(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = '.filesets';
         $result = $bsock->send_command($cmd, 2, null);
         $filesets = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $filesets['result']['filesets'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

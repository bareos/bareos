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

namespace Storage\Model;

class StorageModel
{

   public function getStorages(&$bsock=null)
   {
      if(isset($bsock)) {
         $cmd = 'list storages';
         $result = $bsock->send_command($cmd, 2, null);
         $storages = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $storages['result']['storages'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getStatusStorageSlots(&$bsock=null, $storage=null)
   {
      if(isset($bsock, $storage)) {
         $cmd = 'status storage="' . $storage . '" slots';
         $result = $bsock->send_command($cmd, 2, null);
         $slots = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
         return $slots['result']['contents'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function importSlots(&$bsock=null, $storage=null, $srcslots=null, $dstslots=null)
   {
      if(isset($bsock, $storage)) {
         if($srcslots == null) {
            if($dstslots == null) {
               $cmd = 'import storage="' . $storage;
            }
            else {
               $cmd = 'import storage="' . $storage . '" dstslots=' . $dstslots;
            }
         }
         else {
            if($dstslots == null) {
               $cmd = 'import storage="' . $storage . '" srcslots=' . $srcslots;
            }
            else {
               $cmd = 'import storage="' . $storage . '" srcslots=' . $srcslots . ' dstslots=' . $dstslots;
            }
         }
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function exportSlots(&$bsock=null, $storage=null, $slots=null)
   {
      if(isset($bsock, $storage, $slots)) {
         $cmd = 'export storage="' . $storage . '" srcslots=' . $slots;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function mountSlot(&$bsock=null, $storage=null, $slot=null, $drive=null)
   {
      if(isset($bsock, $storage, $slot, $drive)) {
         $cmd = 'mount storage="' . $storage . '" slot=' . $slot . ' drive=' . $drive;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function unmountSlot(&$bsock=null, $storage=null, $drive=null)
   {
      if(isset($bsock, $storage, $drive)) {
         $cmd = 'unmount storage="' . $storage . '" drive=' . $drive;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function releaseSlot(&$bsock=null, $storage=null, $drive=null)
   {
      if(isset($bsock, $storage, $drive)) {
         $cmd = 'release storage="' . $storage . '" drive=' . $drive;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function updateSlots(&$bsock=null, $storage=null)
   {
      if(isset($bsock, $storage)) {
         $cmd = 'update slots storage="' . $storage . '"';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function moveSlots(&$bsock=null, $storage=null, $srcslots=null, $dstslots=null)
   {
      if(isset($bsock, $storage, $srcslots, $dstslots)) {
         $cmd = 'move storage="' . $storage . '" srcslots=' . $srcslots . ' dstslots=' . $dstslots;
         $result = $bsock->send_command($cmd, 2, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function label(&$bsock=null, $storage=null, $pool=null, $drive=null, $slots=null)
   {
      if(isset($bsock, $storage, $pool, $drive)) {
         $cmd = 'label storage="' . $storage . '" pool="'.$pool.'" drive="'.$drive.'" barcodes yes';
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getPools(&$bsock=null, $type=null)
   {
      if(isset($bsock, $type)) {
         $cmd = '.pools type="'.$type.'"';
         $pools = $bsock->send_command($cmd, 2, null);
         $result = \Zend\Json\Json::decode($pools, \Zend\Json\Json::TYPE_ARRAY);
         return $result['result']['pools'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function getSlots(&$bsock=null, $storage=null)
   {
      if(isset($bsock, $storage)) {
         $cmd = 'status storage="'.$storage.'" slots';
         $slots = $bsock->send_command($cmd, 2, null);
         $result = \Zend\Json\Json::decode($slots, \Zend\Json\Json::TYPE_ARRAY);
         return $result['result']['contents'];
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }

   public function statusStorage(&$bsock=null, $storage=null)
   {
      if(isset($bsock, $storage)) {
         $cmd = 'status storage="'.$storage;
         $result = $bsock->send_command($cmd, 0, null);
         return $result;
      }
      else {
         throw new \Exception('Missing argument.');
      }
   }
}

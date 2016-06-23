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

use Zend\ServiceManager\ServiceLocatorAwareInterface;
use Zend\ServiceManager\ServiceLocatorInterface;

class StorageModel implements ServiceLocatorAwareInterface
{
   protected $serviceLocator;
   protected $director;

   public function __construct()
   {
   }

   public function setServiceLocator(ServiceLocatorInterface $serviceLocator)
   {
      $this->serviceLocator = $serviceLocator;
   }

   public function getServiceLocator()
   {
      return $this->serviceLocator;
   }

   public function getStorages()
   {
      $cmd = 'list storages';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $storages = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $storages['result']['storages'];
   }

   public function getStatusStorageSlots($storage=null)
   {
      $cmd = 'status storage="' . $storage . '" slots';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      $slots = \Zend\Json\Json::decode($result, \Zend\Json\Json::TYPE_ARRAY);
      return $slots['result']['contents'];
   }

   public function importSlots($storage=null, $srcslots=null, $dstslots=null)
   {
      if($dstslots == null) {
         $cmd = 'import storage="' . $storage . '" srcslots=' . $srcslots;
      }
      else {
         $cmd = 'import storage="' . $storage . '" srcslots=' . $srcslots . ' dstslots=' . $dstslots;
      }
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function exportSlots($storage=null, $slots=null)
   {
      $cmd = 'export storage="' . $storage . '" srcslots=' . $slots;
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function mountSlot($storage=null, $slot=null, $drive=null)
   {
      $cmd = 'mount storage="' . $storage . '" slot=' . $slot . ' drive=' . $drive;
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function unmountSlot($storage=null, $drive=null)
   {
      $cmd = 'unmount storage="' . $storage . '" drive=' . $drive;
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function releaseSlot($storage=null, $drive=null)
   {
      $cmd = 'release storage="' . $storage . '" drive=' . $drive;
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function updateSlots($storage=null)
   {
      $cmd = 'update slots storage="' . $storage . '"';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function moveSlots($storage=null, $srcslots=null, $dstslots=null)
   {
      $cmd = 'move storage="' . $storage . '" srcslots=' . $srcslots . ' dstslots=' . $dstslots;
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 2, null);
      return $result;
   }

   public function label($storage=null, $pool=null, $drive=null, $slots=null)
   {
      // Use Scratch pool by default as long as we don't display a pool selection dialog
      $cmd = 'label storage="' . $storage . '" pool=Scratch drive=0 barcodes yes';
      $this->director = $this->getServiceLocator()->get('director');
      $result = $this->director->send_command($cmd, 0, null);
      return $result;
   }

   public function statusStorage($storage=null)
   {
      if(isset($storage)) {
         $cmd = 'status storage="'.$storage;
         $this->director = $this->getServiceLocator()->get('director');
         $result = $this->director->send_command($cmd, 0, null);
         return $result;
      }
      else {
         return false;
      }
   }
}

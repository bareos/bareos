<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos-webui for the canonical source repository
 * @copyright Copyright (c) 2013-2015 Bareos GmbH & Co. KG (http://www.bareos.org/)
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

namespace Storage\Controller;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\View\Model\ViewModel;
use Zend\Json\Json;

class StorageController extends AbstractActionController
{

   protected $storageModel = null;
   protected $bsock = null;

   public function indexAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      return new ViewModel(array());
   }

   public function detailsAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $action = $this->params()->fromQuery('action');
      $storagename = $this->params()->fromRoute('id');

      if(empty($action)) {
         return new ViewModel(array(
            'storagename' => $storagename
         ));
      }
      elseif($action == "import") {
         $storage = $this->params()->fromQuery('storage');
         $srcslots = $this->params()->fromQuery('srcslots');
         $dstslots = $this->params()->fromQuery('dstslots');
         $result = $this->getStorageModel()->importSlots($storage, $srcslots, $dstslots);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }

      elseif($action == "export") {
         $storage = $this->params()->fromQuery('storage');
         $srcslots = $this->params()->fromQuery('srcslots');
         $result = $this->getStorageModel()->exportSlots($storage, $srcslots);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }
      elseif($action == "mount") {
         $storage = $this->params()->fromQuery('storage');
         $slot = $this->params()->fromQuery('slot');
         $drive = $this->params()->fromQuery('drive');
         $result = $this->getStorageModel()->mountSlot($storage, $slot, $drive);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result'=> $result
         ));
      }
      elseif($action == "unmount") {
         $storage = $this->params()->fromQuery('storage');
         $drive = $this->params()->fromQuery('drive');
         $result = $this->getStorageModel()->unmountSlot($storage, $drive);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }
      elseif($action == "release") {
         $storage = $this->params()->fromQuery('storage');
         $drive = $this->params()->fromQuery('drive');
         $result = $this->getStorageModel()->releaseSlot($storage, $drive);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }
      elseif($action == "label") {
         $storage = $this->params()->fromQuery('storage');
         $pool = $this->params()->fromQuery('label');
         $drive = $this->params()->fromQuery('drive');
         $slots = $this->params()->fromQuery('slots');
         $result = $this->getStorageModel()->label($storage, $pool, $drive, $slots);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }
      elseif($action == "updateslots") {
         $storage = $this->params()->fromQuery('storage');
         $result = $this->getStorageModel()->updateSlots($storage);
         return new ViewModel(array(
            'storagename' => $storagename,
            'result' => $result
         ));
      }
      elseif($action == "labelbarcodes") {
      }

   }

   public function statusAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $storage = $this->params()->fromQuery('storage');

      try {
         $this->bsock = $this->getServiceLocator()->get('director');
         $result = $this->getStorageModel()->statusStorage($this->bsock, $storage);
         $this->bsock->disconnect();
      }
      catch(Exception $e) {
         echo $e->getMessage();
      }

      return new ViewModel(
         array(
            'result' => $result
         )
      );
   }

   public function getDataAction()
   {
      $this->RequestURIPlugin()->setRequestURI();

      if(!$this->SessionTimeoutPlugin()->isValid()) {
         return $this->redirect()->toRoute('auth', array('action' => 'login'), array('query' => array('req' => $this->RequestURIPlugin()->getRequestURI(), 'dird' => $_SESSION['bareos']['director'])));
      }

      $result = null;

      $data = $this->params()->fromQuery('data');
      $storage = $this->params()->fromQuery('storage');

      if($data == "all") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getStorageModel()->getStorages($this->bsock);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }
      elseif($data == "statusslots") {
         try {
            $this->bsock = $this->getServiceLocator()->get('director');
            $result = $this->getStorageModel()->getStatusStorageSlots($this->bsock, $storage);
            $this->bsock->disconnect();
         }
         catch(Exception $e) {
            echo $e->getMessage();
         }
      }

      $response = $this->getResponse();
      $response->getHeaders()->addHeaderLine('Content-Type', 'application/json');

      if(isset($result)) {
         $response->setContent(JSON::encode($result));
      }

      return $response;
   }

   public function getStorageModel()
   {
      if(!$this->storageModel) {
         $sm = $this->getServiceLocator();
         $this->storageModel = $sm->get('Storage\Model\StorageModel');
      }
      return $this->storageModel;
   }

}
